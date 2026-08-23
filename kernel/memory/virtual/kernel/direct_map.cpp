/**
 * @file direct_map.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理 RAM 的最终内核直接映射
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <memory/physical/page_db.h>
#include <memory/virtual/kernel/vm_priv.h>
#include <sustcore/addrspace.h>
#include <tay/static_vector.h>

namespace memory::detail {
    tay::expected<void, tay::error_code> map_direct_regions(
        KernelVm &space, const BootInfoHeader &bootinfo) noexcept {
        using exclusion_list = tay::static_vector<PhyArea, MAX_BOOTINFO_REGIONS>;
        const auto *regions  = bootinfo_regions(&bootinfo);
        const PhyArea kernel_image(kernel_start_paddr(), kernel_end_paddr());

        for (size_t parent_idx = 0; parent_idx < page_db().region_count(); ++parent_idx) {
            const auto &parent = page_db().region(parent_idx).parent;
            exclusion_list exclusions;
            const auto kernel_overlap = intersection(parent, kernel_image);
            if (!kernel_overlap.nullable() && !exclusions.push_back(kernel_overlap))
                return tay::Err(tay::error_code::OVERFLOW_ERROR);

            size_t child_idx = regions[parent_idx].rsvd_idx;
            while (child_idx != parent_idx) {
                if (regions[child_idx].type == MemoryType::BAD_MEMORY &&
                    !exclusions.push_back(regions[child_idx].area))
                    return tay::Err(tay::error_code::OVERFLOW_ERROR);
                const size_t next = regions[child_idx].rsvd_idx;
                if (next == child_idx)
                    break;
                child_idx = next;
            }

            for (size_t index = 1; index < exclusions.size(); ++index) {
                PhyArea key   = exclusions[index];
                size_t cursor = index;
                while (cursor != 0 && key.begin < exclusions[cursor - 1].begin) {
                    exclusions[cursor] = exclusions[cursor - 1];
                    --cursor;
                }
                exclusions[cursor] = key;
            }

            addr_t cursor           = parent.begin.arith();
            const auto map_fragment = [&space](addr_t begin, addr_t end) {
                if (begin >= end)
                    return tay::Ok();
                return space.map(
                    PA2KPA(begin), PhyAddr(begin), end - begin,
                    PageFlags{.readable = true, .writable = true, .executable = false});
            };
            for (const auto &excluded : exclusions) {
                if (cursor < excluded.begin.arith())
                    TAY_TRYV(map_fragment(cursor, excluded.begin.arith()));
                if (cursor < excluded.end.arith())
                    cursor = excluded.end.arith();
            }
            TAY_TRYV(map_fragment(cursor, parent.end.arith()));
        }
        return {};
    }
}  // namespace memory::detail
