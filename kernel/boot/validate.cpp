/**
 * @file validate.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动器输入完整性校验
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/early_internal.h>
#include <boot/sections.h>
#include <libfdt.h>
#include <log.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>

#include <cstddef>

namespace {
    constexpr size_t MAX_EARLY_DTB_SIZE = 2 * 1024 * 1024;

    [[nodiscard]] BOOT_INIT_TEXT bool valid_memory_type(MemoryType type) noexcept {
        return static_cast<u32_t>(type) <= static_cast<u32_t>(MemoryType::BAD_MEMORY);
    }

    [[nodiscard]] BOOT_INIT_TEXT size_t checked_bootinfo_prefix_sz(size_t region_cnt) noexcept {
        constexpr size_t FIXED_SIZE       = sizeof(BootInfoHeader) + sizeof(PhyAddr);
        constexpr size_t MAX_REGION_COUNT = (MAX_BOOTINFO_SIZE - FIXED_SIZE) / sizeof(MemoryRegion);
        if (region_cnt == 0 || region_cnt > MAX_REGION_COUNT || region_cnt > MAX_BOOTINFO_REGIONS) {
            kernel::log::panic("无效的 BootInfo 区域数量");
        }
        return sizeof(BootInfoHeader) + region_cnt * sizeof(MemoryRegion) + sizeof(PhyAddr);
    }

    BOOT_INIT_TEXT void validate_regions(const BootInfoHeader *header) noexcept {
        const auto *regions = bootinfo_regions(header);
        bool visited[MAX_BOOTINFO_REGIONS]{};
        size_t parent_cnt = 0;

        for (size_t idx = 0; idx < header->region_cnt; ++idx) {
            const auto &region = regions[idx];
            const addr_t begin = region.area.begin.arith();
            const addr_t end   = region.area.end.arith();

            if (!valid_memory_type(region.type) || begin >= end ||
                !region.area.begin.aligned<PAGE_SIZE>() || !region.area.end.aligned<PAGE_SIZE>())
            {
                kernel::log::panic("无效的 BootInfo 内存区域");
            }
            if (region.rsvd_idx >= header->region_cnt)
                kernel::log::panic("BootInfo 内存区域链接越界");
            if (region.type == MemoryType::FREE) {
                if (parent_cnt != idx)
                    kernel::log::panic("BootInfo FREE 区域不是前缀");
                ++parent_cnt;
            }
        }
        if (parent_cnt == 0)
            kernel::log::panic("BootInfo 没有 FREE 父区域");

        addr_t previous_parent_end = 0;
        for (size_t parent_idx = 0; parent_idx < parent_cnt; ++parent_idx) {
            const auto &parent = regions[parent_idx];
            if (parent_idx != 0 && parent.area.begin.arith() < previous_parent_end)
                kernel::log::panic("BootInfo FREE 父区域重叠或未排序");
            previous_parent_end = parent.area.end.arith();

            size_t child_idx = parent.rsvd_idx;
            if (child_idx == parent_idx)
                continue;
            addr_t previous_child_end = parent.area.begin.arith();
            while (true) {
                if (child_idx < parent_cnt || child_idx >= header->region_cnt || visited[child_idx])
                    kernel::log::panic("无效的 BootInfo 保留链");
                const auto &child = regions[child_idx];
                if (child.type == MemoryType::FREE ||
                    child.area.begin.arith() < parent.area.begin.arith() ||
                    child.area.end.arith() > parent.area.end.arith() ||
                    child.area.begin.arith() < previous_child_end)
                    kernel::log::panic("无效的 BootInfo 保留子区域");

                visited[child_idx] = true;
                previous_child_end = child.area.end.arith();
                const size_t next  = child.rsvd_idx;
                if (next == child_idx)
                    break;
                if (next <= child_idx)
                    kernel::log::panic("BootInfo 保留链未按正向顺序排列");
                child_idx = next;
            }
        }
        for (size_t idx = parent_cnt; idx < header->region_cnt; ++idx)
            if (!visited[idx])
                kernel::log::panic("孤立的 BootInfo 保留子区域");
    }

    BOOT_INIT_TEXT void validate_fdt(const BootInfoHeader *header) noexcept {
        const addr_t dtb_paddr = bootinfo_fdt(header).arith();
        if (dtb_paddr == 0 || (dtb_paddr & (alignof(u64_t) - 1)) != 0) {
            kernel::log::panic("无效的 FDT 物理地址");
        }

        const auto *dtb = reinterpret_cast<const void *>(PA2KPA(dtb_paddr));
        if (fdt_check_header(dtb) != 0) {
            kernel::log::panic("无效的 FDT 头");
        }

        const auto dtb_sz = static_cast<size_t>(fdt_totalsize(dtb));
        if (dtb_sz == 0 || dtb_sz > MAX_EARLY_DTB_SIZE ||
            dtb_paddr > static_cast<addr_t>(-1) - dtb_sz)
        {
            kernel::log::panic("无效的 FDT 大小");
        }
        if (fdt_check_full(dtb, dtb_sz) != 0) {
            kernel::log::panic("无效的 FDT 结构");
        }

        const addr_t dtb_end  = dtb_paddr + dtb_sz;
        const auto *regions   = bootinfo_regions(header);
        bool region_available = false;
        for (size_t idx = 0; idx < header->region_cnt; ++idx) {
            if (regions[idx].area.begin.arith() <= dtb_paddr &&
                dtb_end <= regions[idx].area.end.arith() && regions[idx].type != MemoryType::FREE)
            {
                region_available = true;
                break;
            }
        }
        if (!region_available) {
            kernel::log::panic("FDT 未被保留内存区域覆盖");
        }
    }
}  // namespace

namespace boot::early_internal {
    BOOT_INIT_TEXT void validate_bootinfo(size_t bsp_hwid, const BootInfoHeader *header) noexcept {
        if (header == nullptr ||
            (reinterpret_cast<addr_t>(header) & (alignof(BootInfoHeader) - 1)) != 0)
        {
            kernel::log::panic("无效的 BootInfo 指针");
        }
        if (header->info_sz < sizeof(BootInfoHeader)) {
            kernel::log::panic("BootInfo 头被截断");
        }
        if (header->info_sz > MAX_BOOTINFO_SIZE) {
            kernel::log::panic("BootInfo 超出静态存储空间");
        }
        if (header->hart_id != bsp_hwid) {
            kernel::log::panic("BootInfo 硬件 ID 与入口参数不匹配");
        }

        const size_t prefix_sz = checked_bootinfo_prefix_sz(header->region_cnt);
        if (header->info_sz < prefix_sz) {
            kernel::log::panic("BootInfo 被截断");
        }

        validate_regions(header);
        validate_fdt(header);
    }
}  // namespace boot::early_internal
