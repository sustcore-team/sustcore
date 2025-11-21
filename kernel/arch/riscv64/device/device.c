/**
 * @file device.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备信息接口
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <arch/riscv64/device/device.h>

#include <inttypes.h>

#include <basec/logger.h>
#include <sus/boot.h>
#include <libfdt.h>

static int errno;

/**
 * @brief 获取最近的设备错误码
 * 
 * @return int 错误码
 */
int device_get_errno(void) {
    return errno;
}

FDTDesc *device_check_initial(void *dtb_ptr) {
    FDTDesc *fdt = (FDTDesc *)dtb_ptr;

    /* 检查魔数 */
    if (fdt_magic(fdt) != FDT_MAGIC) {
        errno = -FDT_ERR_BADMAGIC;
        return nullptr;
    }

    /* 检查版本兼容性 */
    if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION) {
        errno = -FDT_ERR_BADVERSION;
        return nullptr;
    }

    /* 完整检查 */
    errno = fdt_check_header(fdt);
    if (errno != 0)
        return nullptr;
    return fdt;
}

FDTNodeDesc get_device_root(const FDTDesc *fdt) {
    return 0;
}

FDTNodeDesc get_sub_device(const FDTDesc *fdt, FDTNodeDesc parent, const char *name) {
    FDTNodeDesc child;
    // 遍历并匹配
    fdt_for_each_subnode(child, fdt, parent) {
        const char *node_name = fdt_get_name(fdt, child, NULL);
        // 形如"name"或"name@addr"均可匹配
        if (strcmp(node_name, name) == 0) {
            return child;
        }
        else if (strncmp(   node_name, name, strlen(name)) == 0 && node_name[strlen(name)] == '@') {
            return child;
        }
    }
    return -1; // 未找到
}

FDTPropDesc get_device_property(const FDTDesc *fdt, FDTNodeDesc node, const char *name) {
    FDTPropDesc prop;
    // 遍历并匹配
    fdt_for_each_property_offset(prop, fdt, node) {
        const char *prop_name;
        fdt_getprop_by_offset(fdt, prop, &prop_name, NULL);
        if (strcmp(prop_name, name) == 0) {
            return prop;
        }
    }
    return -1; // 未找到
}

FDTPropVal get_device_property_value(const FDTDesc *fdt, FDTPropDesc prop) {
    FDTPropVal val;
    const char *prop_name;
    val.ptr = (void *)fdt_getprop_by_offset(fdt, prop, &prop_name, &val.len);
    return val;
}

const char *get_device_property_value_as_string(const FDTDesc *fdt, FDTPropDesc prop) {
    FDTPropVal val = get_device_property_value(fdt, prop);
    return (const char *)val.ptr;
}

byte get_device_property_value_as_byte(const FDTDesc *fdt, FDTPropDesc prop) {
    FDTPropVal val = get_device_property_value(fdt, prop);
    if (val.len >= 1) {
        return *(const byte *)val.ptr;
    }
    return 0;
}

word get_device_property_value_as_word(const FDTDesc *fdt, FDTPropDesc prop) {
    FDTPropVal val = get_device_property_value(fdt, prop);
    if (val.len >= sizeof(word)) {
        return fdt16_to_cpu(*(const word *)val.ptr);
    }
    return 0;
}

dword get_device_property_value_as_dword(const FDTDesc *fdt, FDTPropDesc prop) {
    FDTPropVal val = get_device_property_value(fdt, prop);
    if (val.len >= sizeof(dword)) {
        return fdt32_to_cpu(*(const dword *)val.ptr);
    }
    return 0;
}

qword get_device_property_value_as_qword(const FDTDesc *fdt, FDTPropDesc prop) {
    FDTPropVal val = get_device_property_value(fdt, prop);
    if (val.len >= sizeof(qword)) {
        return fdt64_to_cpu(*(const qword *)val.ptr);
    }
    return 0;
}

int get_reg_region_number(const FDTDesc *fdt, FDTPropDesc prop, int addr_cells, int size_cells)
{
    FDTPropVal val = get_device_property_value(fdt, prop);
    int cells_per_region = addr_cells + size_cells;
    int bytes_per_region = cells_per_region * sizeof(fdt32_t);

    if (val.len % bytes_per_region != 0) {
        return -1; // 无效的 reg 属性长度
    }

    int num_regions = val.len / bytes_per_region;
    return num_regions;
}

int get_device_property_value_as_reg_regions(const FDTDesc *fdt, FDTPropDesc prop, int addr_cells, int size_cells,
    FDTRegVal *regions, int max_regions)
{
    FDTPropVal val = get_device_property_value(fdt, prop);
    // 首先计算区域内的cells数, 与每个区域的大小
    int cells_per_region = addr_cells + size_cells;
    int bytes_per_region = cells_per_region * sizeof(fdt32_t);

    if (val.len % bytes_per_region != 0) {
        return -1; // 无效的 reg 属性长度
    }

    int num_regions = val.len / bytes_per_region;
    if (num_regions > max_regions) {
        num_regions = max_regions; // 限制最大区域数量
    }

    // 开始处理区域内的cells
    const fdt32_t *cells = (const fdt32_t *)val.ptr;

    // 逐区域处理
    for (int i = 0; i < num_regions; i++) {
        umb_t address = 0;
        umb_t size = 0;

        // 解析地址
        for (int j = 0; j < addr_cells; j++) {
            address = (address << 32) | fdt32_to_cpu(cells[i * cells_per_region + j]);
        }

        // 解析大小
        for (int j = 0; j < size_cells; j++) {
            size = (size << 32) | fdt32_to_cpu(cells[i * cells_per_region + addr_cells + j]);
        }

        // 存储地址和大小
        regions[i].ptr = (void *)address;
        regions[i].size = (size_t)size;
    }

    return num_regions;
}

int get_parent_address_cells(const FDTDesc *fdt, FDTNodeDesc node) {
    int addr_cells = fdt_address_cells(fdt, fdt_parent_offset(fdt, node));
    if (addr_cells < 0) {
        addr_cells = 2; // 默认值
    }
    return addr_cells;
}

int get_parent_size_cells(const FDTDesc *fdt, FDTNodeDesc node) {
    int size_cells = fdt_size_cells(fdt, fdt_parent_offset(fdt, node));
    if (size_cells < 0) {
        size_cells = 2; // 默认值
    }
    return size_cells;
}

// 设备树打印:打印缩进
static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        kprintf("  ");
    }
}

// 打印属性值（根据属性类型）
static void print_property_value(const char *name, const void *value, int len) {
    // 根据属性名称和长度判断类型并打印
    if (strcmp(name, "compatible") == 0 || 
        strcmp(name, "model") == 0 ||
        strcmp(name, "status") == 0 ||
        strcmp(name, "name") == 0 ||
        strcmp(name, "device_type") == 0) {
        // 字符串属性
        const char *str = value;
        int printed = 0;
        while (printed < len) {
            kprintf("\"%s\"", str);
            printed += strlen(str) + 1;
            str += strlen(str) + 1;
            if (printed < len) {
                kprintf(", ");
            }
        }
    } else if (strcmp(name, "reg") == 0) {
        // reg 属性 - 特殊处理
        kprintf("<寄存器>");
    } else if (strcmp(name, "interrupts") == 0) {
        // interrupts 属性 - 特殊处理
        kprintf("<中断>");
    } else if (strcmp(name, "phandle") == 0 && len == 4) {
        // phandle
        dword phandle = fdt32_to_cpu(*(const dword *)value);
        kprintf("%" PRIu32, phandle);
    } else if (len == 2) {
        // 16位整数
        word val = fdt32_to_cpu(*(const word *)value);
        kprintf("0x%x (%u) (%d bits)", (dword)val, (dword)val, 16);
    } else if (len == 4) {
        // 32位整数
        dword val = fdt32_to_cpu(*(const dword *)value);
        kprintf("0x%x (%u) (%d bits)", val, val, 32);
    } else if (len == 8) {
        // 64位整数
        qword val = fdt64_to_cpu(*(const qword *)value);
        kprintf("0x%llx (%llu) (%d bits)", val, val, 64);
    } else if (len == 1) {
        // 字节
        byte val = *(const byte *)value;
        kprintf("0x%02x (%u) (%d bits)", (dword)val, (dword)val, 8);
    } else {
        // 二进制数据
        kprintf("[");
        for (int i = 0; i < len && i < 16; i++) {
            kprintf("%02x", ((const unsigned char *)value)[i]);
            if (i < len - 1 && i < 15) {
                kprintf(" ");
            }
        }
        // 长度超过16字节时省略显示
        if (len > 16) {
            kprintf("...");
        }
        kprintf("] (%d bytes)", len);
    }
}

static void print_reg_property(const char *name, int addr_cells, int size_cells, FDTPropVal prop_reg, int depth) {
    int cells_per_region = addr_cells + size_cells;
    int bytes_per_region = cells_per_region * sizeof(fdt32_t);

    if (prop_reg.len % bytes_per_region != 0) {
        print_indent(depth);
        kprintf("%s = 无效的 reg 属性长度\n", name);
        return;
    }

    int num_regions = prop_reg.len / bytes_per_region;
    const fdt32_t *cells = (const fdt32_t *)prop_reg.ptr;

    print_indent(depth);
    kprintf("%s = <\n", name);

    for (int i = 0; i < num_regions; i++) {
        qword address = 0;
        qword size = 0;

        // 解析地址
        for (int j = 0; j < addr_cells; j++) {
            address = (address << 32) | fdt32_to_cpu(cells[i * cells_per_region + j]);
        }

        // 解析大小
        for (int j = 0; j < size_cells; j++) {
            size = (size << 32) | fdt32_to_cpu(cells[i * cells_per_region + addr_cells + j]);
        }

        print_indent(depth + 1);
        kprintf("  区域 %d: 地址 = 0x%016x, 大小 = 0x%016x \n",
            i, address, size);
    }
    print_indent(depth);
    kprintf(">;\n");
}

// 递归打印节点及其所有属性
static void print_node_recursive(const FDTDesc *fdt, FDTNodeDesc node, int depth) {
    const char *node_name = fdt_get_name(fdt, node, NULL);

    // 打印节点名称
    print_indent(depth);
    if (depth == 0)
        kprintf("/ {\n");
    else
        kprintf("%s {\n", node_name);

    FDTPropDesc prop;
    FDTNodeDesc child;

    // 遍历并打印所有属性
    fdt_for_each_property_offset(prop, fdt, node) {
        // 获得属性
        const char *prop_name;
        const void *prop_value;
        int prop_len;

        prop_value = fdt_getprop_by_offset(fdt, prop, &prop_name, &prop_len);

        if (strcmp(prop_name, "reg") == 0) {
            // 暂时跳过
            continue;
        }

        // 打印属性名称和值
        if (prop_value) {
            print_indent(depth + 1);
            kprintf("%s = ", prop_name);
            print_property_value(prop_name, prop_value, prop_len);
            kprintf(";\n");
        }
        else {
            print_indent(depth + 1);
            kprintf("异常属性;\n");
        }
    }

    FDTPropDesc prop_regs = get_device_property(fdt, node, "reg");
    if (prop_regs != -1) {
        // 获取父节点的#address-cells和#size-cells
        FDTNodeDesc parent = fdt_parent_offset(fdt, node);
        if (parent >= 0) {
            int addr_cells = fdt_address_cells(fdt, parent);
            int size_cells = fdt_size_cells(fdt, parent);

            if (addr_cells <= 0) {
                addr_cells = 2; // 默认值
            }
            if (size_cells <= 0) {
                size_cells = 2; // 默认值
            }

            print_reg_property("reg", addr_cells, size_cells, get_device_property_value(fdt, prop_regs), depth + 1);
        }
    }

    // 递归处理所有子节点
    fdt_for_each_subnode(child, fdt, node) {
        print_node_recursive(fdt, child, depth + 1);
    }

    print_indent(depth);
    kprintf("}\n");
}

// 打印内存保留区域
static void print_memory_reservations(const FDTDesc *fdt) {
    kprintf("/* 内存保留区域 */\n");

    qword address, size;
    int offset = 0;

    while (true) {
        // 若果没有更多保留区域则退出
        if (fdt_get_mem_rsv(fdt, offset, &address, &size) < 0) {
            break;
        }
        
        if (address == 0 && size == 0) {
            break;
        }
        
        // 打印保留区域的起始和结束地址
        kprintf(" 0x%" PRIx64 " 0x%" PRIx64, address, address + size - 1);
        offset++;
    }
}

// 统计函数
static void count_nodes_props(const FDTDesc *fdt, FDTNodeDesc node, int *nodes, int *props) {
    (*nodes)++;
    
    FDTPropDesc prop;
    FDTNodeDesc child;

    // 遍历属性
    fdt_for_each_property_offset(prop, fdt, node) {
        (*props)++;
    }
    
    // 递归遍历子节点
    fdt_for_each_subnode(child, fdt, node) {
        count_nodes_props(fdt, child, nodes, props);
    }
}

// 主函数：打印整个设备树的所有信息
void print_entire_device_tree(const FDTDesc *fdt) {
    // 检查设备树有效性
    if (fdt_check_header(fdt) != 0) {
        log_error("无效的二进制设备树\n");
        return;
    }
    
    kprintf("设备树 \n");
    kprintf("总大小: %d 字节\n\n", fdt_totalsize(fdt));
    
    // 打印内存保留区域
    print_memory_reservations(fdt);
    
    // 打印根节点及其所有子节点
    print_node_recursive(fdt, get_device_root(fdt), 0);
}

// 附加功能
void print_device_tree_detailed(const FDTDesc *fdt) {
    if (fdt_check_header(fdt) != 0) {
        log_error("无效的二进制设备树\n");
        return;
    }
    
    kprintf("=== 设备树详细信息 ===\n\n");
    
    // 头部信息
    kprintf("  设备树文件头:\n");
    kprintf("  魔数: 0x%08x\n", fdt_magic(fdt));
    kprintf("  总大小: %d 字节\n", fdt_totalsize(fdt));
    kprintf("  结构块偏移: %d\n", fdt_off_dt_struct(fdt));
    kprintf("  字符串块偏移: %d\n", fdt_off_dt_strings(fdt));
    kprintf("  内存保留偏移: %d\n", fdt_off_mem_rsvmap(fdt));
    kprintf("  版本: %d\n", fdt_version(fdt));
    kprintf("  最后兼容版本: %d\n", fdt_last_comp_version(fdt));
    kprintf("  启动CPU ID: %d\n", fdt_boot_cpuid_phys(fdt));
    kprintf("  字符串块大小: %d\n", fdt_size_dt_strings(fdt));
    kprintf("  结构块大小: %d\n", fdt_size_dt_struct(fdt));
    kprintf("\n");
    
    // 内存保留区域详细信息
    kprintf("内存保留区域:\n");
    int i = 0;
    qword address, size;
    while (fdt_get_mem_rsv(fdt, i, &address, &size) >= 0) {
        if (address == 0 && size == 0) break;
        kprintf("  保留区域 %d: 0x%016" PRIx64 " - 0x%016" PRIx64 " (大小: 0x%" PRIx64 " 字节)\n",
               i, address, address + size - 1, size);
        i++;
    }
    if (i == 0) {
        kprintf("  无内存保留区域\n");
    }
    kprintf("\n");
    
    // 节点统计
    kprintf("节点统计:\n");
    int total_nodes = 0;
    int total_properties = 0;
    
    count_nodes_props(fdt, get_device_root(fdt), &total_nodes, &total_properties);
    kprintf("  总节点数: %d\n", total_nodes);
    kprintf("  总属性数: %d\n", total_properties);
    kprintf("\n");
    
    // 打印完整的设备树结构
    kprintf("完整的设备树结构:\n");
    kprintf("================================\n\n");
    print_entire_device_tree(fdt);
}