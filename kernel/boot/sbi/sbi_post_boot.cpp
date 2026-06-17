/**
 * @file sbi_post_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI 启动代码第二部分
 * @version alpha-1.0.0
 * @date 2026-06-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/sbi/sbi_paging.h>
#include <sbi/sbi.h>
#include <sustcore/boot.h>

#include <cstring>
#include <libfdt.h>

#define _SBI_FUNCTION  SECTION(".sbi_post_boot.text")
#define _SBI_DATA      SECTION(".sbi_post_boot.data")
#define _SBI_STRING(x) _SBI_DATA constexpr const char x[]

namespace sbi {
    _SBI_STRING(SBI_POST_BOOT_MSG) = "SBI引导程序第二部分启动!\n";
    _SBI_STRING(SBI_KERNEL_ENTRY_MSG) = "SBI引导程序进入内核入口!\n";
    _SBI_STRING(SBI_BOOTINFO_OVERFLOW_MSG) = "错误: BootInfo 区域数量超限\n";
    _SBI_STRING(SBI_BOOTINFO_ALLOC_MSG) = "错误: SBI reclaimable 区域不足\n";
    _SBI_STRING(SBI_INVALID_DTB_MSG) = "错误: FDT 无效\n";
    _SBI_STRING(SBI_BOOTINFO_TOO_LARGE_MSG) = "错误: BootInfo 超过 128KB 限制\n";
    _SBI_STRING(SBI_BOOTINFO_MISALIGNED_REGION_MSG) =
        "错误: BootInfo 存在未对齐到4KB的内存区域\n";

    constexpr size_t MAX_BOOTINFO_REGIONS = 128;
    constexpr size_t CELL_SIZE            = sizeof(fdt32_t);

    extern "C" void _start(size_t hart_id, BootInfoHeader *bootinfo);

    _SBI_FUNCTION void sbi_writes(const char *str) {
        int len = strlen(str);
        for (int i = 0; i < len; i++) {
            sbi_dbcn_console_write_byte(str[i]);
        }
    }

    _SBI_FUNCTION [[noreturn]] void post_panic(const char *msg) {
        sbi_writes(msg);
        while (true);
    }

    _SBI_FUNCTION addr_t kva_to_pa(const char *ptr) {
        return reinterpret_cast<addr_t>(ptr) - KVA_OFFSET;
    }

    _SBI_FUNCTION bool region_empty(const MemRegion &region) {
        return region.area.nullable();
    }

    _SBI_FUNCTION addr_t region_end(const MemRegion &region) {
        return region.area.end.arith();
    }

    _SBI_FUNCTION bool region_page_aligned(const MemRegion &region) {
        return region.area.begin.aligned<PAGE_SIZE>() &&
               region.area.end.aligned<PAGE_SIZE>();
    }

    _SBI_FUNCTION PhyArea page_aligned_dtb_area(addr_t dtb_ptr,
                                                size_t dtb_size) {
        return page_align_outward(
            PhyArea(PhyAddr(dtb_ptr), PhyAddr(dtb_ptr + dtb_size)));
    }

    _SBI_FUNCTION void append_region(MemRegion *regions, size_t &cnt,
                                     PhyArea area,
                                     MemRegion::MemoryStatus status) {
        if (area.nullable()) {
            return;
        }
        if (cnt >= MAX_BOOTINFO_REGIONS) {
            post_panic(SBI_BOOTINFO_OVERFLOW_MSG);
        }
        regions[cnt] = MemRegion{
            .status = status,
            .area   = area,
        };
        ++cnt;
    }

    _SBI_FUNCTION bool region_less(const MemRegion &lhs,
                                   const MemRegion &rhs) {
        if (lhs.area.begin != rhs.area.begin) {
            return lhs.area.begin < rhs.area.begin;
        }
        if (lhs.area.end != rhs.area.end) {
            return lhs.area.end < rhs.area.end;
        }
        return static_cast<int>(lhs.status) < static_cast<int>(rhs.status);
    }

    _SBI_FUNCTION void sort_regions(MemRegion *regions, size_t cnt) {
        for (size_t i = 1; i < cnt; ++i) {
            MemRegion key = regions[i];
            size_t j      = i;
            while (j > 0 && region_less(key, regions[j - 1])) {
                regions[j] = regions[j - 1];
                --j;
            }
            regions[j] = key;
        }
    }

    _SBI_FUNCTION bool mergeable(const MemRegion &lhs, const MemRegion &rhs) {
        return lhs.status == rhs.status &&
               region_end(lhs) >= rhs.area.begin.arith();
    }

    _SBI_FUNCTION size_t merge_regions(MemRegion *regions, size_t cnt) {
        if (cnt == 0) {
            return 0;
        }
        sort_regions(regions, cnt);
        size_t dst = 0;
        for (size_t i = 0; i < cnt; ++i) {
            if (region_empty(regions[i])) {
                continue;
            }
            if (dst > 0 && mergeable(regions[dst - 1], regions[i])) {
                addr_t end = region_end(regions[i]);
                if (region_end(regions[dst - 1]) < end) {
                    regions[dst - 1].area.end = PhyAddr(end);
                }
                continue;
            }
            regions[dst++] = regions[i];
        }
        return dst;
    }

    _SBI_FUNCTION void validate_final_regions(const MemRegion *regions,
                                              size_t cnt) {
        for (size_t i = 0; i < cnt; ++i) {
            if (region_end(regions[i]) < regions[i].area.begin.arith() ||
                !region_page_aligned(regions[i]))
            {
                post_panic(SBI_BOOTINFO_MISALIGNED_REGION_MSG);
            }
        }
    }

    _SBI_FUNCTION uint64_t read_be(const void *data, size_t cells) {
        const auto *value = static_cast<const fdt32_t *>(data);
        uint64_t result   = 0;
        for (size_t i = 0; i < cells; ++i) {
            result = (result << 32) | fdt32_to_cpu(value[i]);
        }
        return result;
    }

    _SBI_FUNCTION int parent_cell_count(const void *dtb, int node,
                                        const char *name, int fallback) {
        int parent = fdt_parent_offset(dtb, node);
        if (parent < 0) {
            parent = 0;
        }
        int len          = 0;
        const auto *prop = static_cast<const fdt32_t *>(
            fdt_getprop(dtb, parent, name, &len));
        if (prop == nullptr || len != static_cast<int>(sizeof(fdt32_t))) {
            return fallback;
        }
        return static_cast<int>(fdt32_to_cpu(*prop));
    }

    _SBI_FUNCTION bool node_enabled(const void *dtb, int node) {
        int len            = 0;
        const char *status = static_cast<const char *>(
            fdt_getprop(dtb, node, "status", &len));
        if (status == nullptr) {
            return true;
        }
        return strcmp(status, "okay") == 0 || strcmp(status, "ok") == 0;
    }

    _SBI_FUNCTION void append_reg_regions(const void *dtb, int node,
                                          MemRegion *regions, size_t &cnt,
                                          MemRegion::MemoryStatus status) {
        int len            = 0;
        const void *reg    = fdt_getprop(dtb, node, "reg", &len);
        int addr_cells     = parent_cell_count(dtb, node, "#address-cells", 2);
        int size_cells     = parent_cell_count(dtb, node, "#size-cells", 1);
        int cells_per_item = addr_cells + size_cells;
        if (reg == nullptr || len <= 0 || cells_per_item <= 0 ||
            len % static_cast<int>(cells_per_item * CELL_SIZE) != 0)
        {
            return;
        }

        const char *cursor = static_cast<const char *>(reg);
        for (int offset = 0; offset < len;
             offset += cells_per_item * static_cast<int>(CELL_SIZE))
        {
            addr_t begin = static_cast<addr_t>(
                read_be(cursor + offset, static_cast<size_t>(addr_cells)));
            size_t size = static_cast<size_t>(read_be(
                cursor + offset + addr_cells * CELL_SIZE,
                static_cast<size_t>(size_cells)));
            append_region(regions, cnt,
                          PhyArea(PhyAddr(begin), PhyAddr(begin + size)),
                          status);
        }
    }

    _SBI_FUNCTION void collect_memory_regions(const void *dtb,
                                              MemRegion *regions,
                                              size_t &cnt) {
        int node = -1;
        while ((node = fdt_next_node(dtb, node, nullptr)) >= 0) {
            if (!node_enabled(dtb, node)) {
                continue;
            }
            int len                  = 0;
            const char *device_type  = static_cast<const char *>(
                fdt_getprop(dtb, node, "device_type", &len));
            if (device_type == nullptr || strcmp(device_type, "memory") != 0) {
                continue;
            }
            append_reg_regions(dtb, node, regions, cnt,
                               MemRegion::MemoryStatus::FREE);
        }
    }

    _SBI_FUNCTION void collect_reserved_regions(const void *dtb,
                                                MemRegion *regions,
                                                size_t &cnt) {
        int reserved = fdt_path_offset(dtb, "/reserved-memory");
        if (reserved < 0) {
            return;
        }

        int child = 0;
        fdt_for_each_subnode(child, dtb, reserved) {
            if (!node_enabled(dtb, child)) {
                continue;
            }
            append_reg_regions(dtb, child, regions, cnt,
                               MemRegion::MemoryStatus::RESERVED);
        }
    }

    _SBI_FUNCTION void append_free_fragment(MemRegion *final_regions,
                                            size_t &final_cnt, addr_t begin,
                                            addr_t end) {
        if (begin < end) {
            append_region(final_regions, final_cnt,
                          PhyArea(PhyAddr(begin), PhyAddr(end)),
                          MemRegion::MemoryStatus::FREE);
        }
    }

    _SBI_FUNCTION size_t normalize_boot_regions(MemRegion *memory_regions,
                                                size_t memory_cnt,
                                                MemRegion *reserved_regions,
                                                size_t reserved_cnt,
                                                MemRegion *final_regions) {
        memory_cnt   = merge_regions(memory_regions, memory_cnt);
        reserved_cnt = merge_regions(reserved_regions, reserved_cnt);

        size_t final_cnt = 0;
        for (size_t i = 0; i < reserved_cnt; ++i) {
            final_regions[final_cnt++] = reserved_regions[i];
        }

        for (size_t i = 0; i < memory_cnt; ++i) {
            addr_t current = memory_regions[i].area.begin.arith();
            addr_t end     = region_end(memory_regions[i]);

            for (size_t j = 0; j < reserved_cnt; ++j) {
                addr_t reserved_begin = reserved_regions[j].area.begin.arith();
                addr_t reserved_end   = region_end(reserved_regions[j]);
                if (reserved_end <= current) {
                    continue;
                }
                if (end <= reserved_begin) {
                    break;
                }
                append_free_fragment(final_regions, final_cnt, current,
                                     reserved_begin);
                if (current < reserved_end) {
                    current = reserved_end;
                }
                if (current >= end) {
                    break;
                }
            }
            append_free_fragment(final_regions, final_cnt, current, end);
        }

        return merge_regions(final_regions, final_cnt);
    }

    _SBI_FUNCTION BootInfoHeader *build_bootinfo(addr_t dtb_ptr,
                                                 addr_t cursor) {
        MemRegion memory_regions[MAX_BOOTINFO_REGIONS];
        MemRegion reserved_regions[MAX_BOOTINFO_REGIONS];
        MemRegion final_regions[MAX_BOOTINFO_REGIONS];
        size_t memory_cnt   = 0;
        size_t reserved_cnt = 0;

        const void *dtb = reinterpret_cast<const void *>(dtb_ptr);
        if (fdt_check_header(dtb) != 0) {
            post_panic(SBI_INVALID_DTB_MSG);
        }
        size_t dtb_size = static_cast<size_t>(fdt_totalsize(dtb));
        collect_memory_regions(dtb, memory_regions, memory_cnt);
        collect_reserved_regions(dtb, reserved_regions, reserved_cnt);
        append_region(reserved_regions, reserved_cnt,
                      page_aligned_dtb_area(dtb_ptr, dtb_size),
                      MemRegion::MemoryStatus::BOOT_RECLAIMABLE);
        append_region(memory_regions, memory_cnt,
                      PhyArea(PhyAddr(kva_to_pa(&s_sbi_kva)),
                              PhyAddr(kva_to_pa(&s_sbi_reclaimable_kva))),
                      MemRegion::MemoryStatus::FREE);
        append_region(reserved_regions, reserved_cnt,
                      PhyArea(PhyAddr(kva_to_pa(&s_sbi_reclaimable_kva)),
                              PhyAddr(kva_to_pa(&e_sbi_reclaimable_kva))),
                      MemRegion::MemoryStatus::BOOT_RECLAIMABLE);
        append_region(reserved_regions, reserved_cnt,
                      PhyArea(PhyAddr(kva_to_pa(&e_sbi_reclaimable_kva)),
                              PhyAddr(kva_to_pa(&ekernel))),
                      MemRegion::MemoryStatus::RESERVED);

        size_t final_cnt = normalize_boot_regions(
            memory_regions, memory_cnt, reserved_regions, reserved_cnt,
            final_regions);
        validate_final_regions(final_regions, final_cnt);

        addr_t aligned_cursor = (cursor + PAGE_TABLE_ALIGNMENT - 1) &
                                ~(PAGE_TABLE_ALIGNMENT - 1);
        size_t info_size = sizeof(BootInfoHeader) +
                           sizeof(MemRegion) * final_cnt + sizeof(PhyAddr);
        if (info_size > MAX_BOOTINFO_SIZE) {
            post_panic(SBI_BOOTINFO_TOO_LARGE_MSG);
        }
        if (aligned_cursor + info_size > kva_to_pa(&e_sbi_reclaimable_kva)) {
            post_panic(SBI_BOOTINFO_ALLOC_MSG);
        }

        auto *header = reinterpret_cast<BootInfoHeader *>(aligned_cursor);
        header->info_sz    = info_size;
        header->region_cnt = final_cnt;
        auto *regions      = bootinfo_regions(header);
        for (size_t i = 0; i < final_cnt; ++i) {
            regions[i] = final_regions[i];
        }
        *bootinfo_fdt_pa(header) = PhyAddr(dtb_ptr);
        return header;
    }

    extern "C" _SBI_FUNCTION void _sbi_post_start(size_t hart_id,
                                                  addr_t dtb_ptr,
                                                  addr_t reclaimable_cursor) {
        sbi_writes(SBI_POST_BOOT_MSG);
        auto *bootinfo = build_bootinfo(dtb_ptr, reclaimable_cursor);
        bootinfo->hart_id = hart_id;
        sbi_writes(SBI_KERNEL_ENTRY_MSG);
        _start(hart_id, bootinfo);
        while (true);
    }
}  // namespace sbi
