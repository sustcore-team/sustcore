/**
 * @file fdt_helper.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT Helper 实现
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/fdt_helper.h>
#include <libfdt.h>

namespace FDTHelper {
    FDTDesc *fdt = nullptr;
    int errno    = 0;

    FDTDesc *fdt_init(void *dtb_ptr) {
        fdt = (FDTDesc *)dtb_ptr;

        // 检查FDT魔数
        if (fdt_magic(fdt) != FDT_MAGIC) {
            errno = -FDT_ERR_BADMAGIC;  // Invalid FDT
            return nullptr;
        }

        // 检查版本兼容性
        if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION ||
            fdt_version(fdt) > FDT_LAST_SUPPORTED_VERSION)
        {
            errno = -FDT_ERR_BADVERSION;  // Unsupported version
            return nullptr;
        }

        // 完整检查
        errno = fdt_check_header(fdt);
        return (errno == 0) ? fdt : nullptr;
    }

    FDTNodeDesc get_root_node(void) {
        return 0;
    }

    FDTNodeDesc get_subnode(FDTNodeDesc parent, const char *name) {
        FDTNodeDesc child;
        fdt_for_each_subnode(child, fdt, parent) {
            const char *node_name = fdt_get_name(fdt, child, nullptr);
            // 形如"name"或"name@addr"均可匹配
            if (strcmp(node_name, name) == 0) {
                return child;
            } else if (strncmp(node_name, name, strlen(name)) == 0 &&
                       node_name[strlen(name)] == '@')
            {
                return child;
            }
        }
        return -1;  // Not found
    }

    FDTPropDesc get_property(FDTNodeDesc node, const char *name) {
        FDTPropDesc prop;
        // 遍历并匹配
        fdt_for_each_property_offset(prop, fdt, node) {
            const char *prop_name;
            fdt_getprop_by_offset(fdt, prop, &prop_name, nullptr);
            if (strcmp(prop_name, name) == 0) {
                return prop;
            }
        }
        return -1;  // 未找到
    }

    PropVal get_property_value(FDTPropDesc prop) {
        PropVal val;
        int len;
        val.ptr = (void *)fdt_getprop_by_offset(fdt, prop, nullptr, &len);
        val.len = (size_t)len;
        return val;
    }

    const char *get_property_value_as_string(FDTPropDesc prop) {
        return (const char *)(get_property_value(prop).ptr);
    }

    template<>
    word get_property_value_as<word, 0>(FDTPropDesc prop) {
        PropVal val = get_property_value(prop);
        if (val.len < sizeof(word)) {
            return 0;
        }
        return fdt16_to_cpu(*(const word *)val.ptr);
    }
    template<>
    dword get_property_value_as<dword, 0>(FDTPropDesc prop) {
        PropVal val = get_property_value(prop);
        if (val.len < sizeof(word)) {
            return 0;
        }
        return fdt32_to_cpu(*(const dword *)val.ptr);
    }
    template<>
    int get_property_value_as<int, -1>(FDTPropDesc prop) {
        union {
            int i;
            dword d;
        };
        d = get_property_value_as<dword>(prop);
        return i;
    }
    template<>
    qword get_property_value_as<qword, 0>(FDTPropDesc prop) {
        PropVal val = get_property_value(prop);
        if (val.len < sizeof(qword)) {
            return 0;
        }
        return fdt64_to_cpu(*(const qword *)val.ptr);
    }

    int get_reg_regions_cnt(FDTPropDesc prop, int addr_cells, int size_cells) {
        PropVal val          = get_property_value(prop);
        int cells_per_region = addr_cells + size_cells;
        int bytes_per_region = cells_per_region * sizeof(fdt32_t);

        if (val.len % bytes_per_region != 0) {
            return -1;  // 无效的 reg 属性长度
        }

        int num_regions = val.len / bytes_per_region;
        return num_regions;
    }

    int get_property_value_as_reg_regions(FDTPropDesc prop, int addr_cells,
                                          int size_cells, RegVal *regions,
                                          int max_regions) {
        PropVal val          = get_property_value(prop);
        // 首先计算区域内的cells数, 与每个区域的大小
        int cells_per_region = addr_cells + size_cells;
        int bytes_per_region = cells_per_region * sizeof(fdt32_t);

        if (val.len % bytes_per_region != 0) {
            return -1;  // 无效的 reg 属性长度
        }

        int num_regions = val.len / bytes_per_region;
        if (num_regions > max_regions) {
            num_regions = max_regions;  // 限制最大区域数量
        }

        // 开始处理区域内的cells
        const fdt32_t *cells = (const fdt32_t *)val.ptr;

        // 逐区域处理
        for (int i = 0; i < num_regions; i++) {
            umb_t address = 0;
            umb_t size    = 0;

            // 解析地址
            for (int j = 0; j < addr_cells; j++) {
                address = (address << 32) |
                          fdt32_to_cpu(cells[i * cells_per_region + j]);
            }

            // 解析大小
            for (int j = 0; j < size_cells; j++) {
                size =
                    (size << 32) |
                    fdt32_to_cpu(cells[i * cells_per_region + addr_cells + j]);
            }

            // 存储地址和大小
            regions[i].ptr  = (void *)address;
            regions[i].size = (size_t)size;
        }

        return num_regions;
    }

    int get_parent_addresses_cells(FDTNodeDesc node) {
        int addr_cells = fdt_address_cells(fdt, fdt_parent_offset(fdt, node));
        if (addr_cells < 0) {
            addr_cells = 2;  // 默认值
        }
        return addr_cells;
    }

    int get_parent_size_cells(FDTNodeDesc node) {
        int size_cells = fdt_size_cells(fdt, fdt_parent_offset(fdt, node));
        if (size_cells < 0) {
            size_cells = 2;  // 默认值
        }
        return size_cells;
    }
}  // namespace FDTHelper