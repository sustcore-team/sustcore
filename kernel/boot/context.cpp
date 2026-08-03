/**
 * @file context.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动信息持久化与内存所有权管理
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/common/bytes.h>
#include <boot/context.h>
#include <libfdt.h>
#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/physical/page_database.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>

#include <cstddef>

namespace boot {
    constinit Context saved_context;

    namespace {
        extern "C" char s_init[], e_init[];
        constexpr u64_t BOOTINFO_OWNER = 2;
        constexpr u64_t FDT_OWNER      = 3;

        constexpr size_t pages_for(size_t bytes) noexcept {
            return (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        }

    }  // namespace

    Context &context() noexcept {
        return saved_context;
    }

    void preserve(const BootInfoHeader &source) noexcept {
        if (saved_context.info != nullptr) {
            kernel::log::panic("BootContext 被重复初始化");
        }

        const auto source_fdt_paddr = bootinfo_fdt(&source).arith();
        const auto *source_fdt      = reinterpret_cast<const void *>(PA2KPA(source_fdt_paddr));
        if (fdt_check_header(source_fdt) != 0) {
            kernel::log::panic("无法持久化无效的 FDT");
        }
        const auto fdt_sz = static_cast<size_t>(fdt_totalsize(source_fdt));

        auto info_allocation = memory::buddy()->try_get_free_pages(pages_for(source.info_sz));
        if (!info_allocation) {
            kernel::log::panic("无法分配持久化 BootInfo");
        }
        auto fdt_allocation = memory::buddy()->try_get_free_pages(pages_for(fdt_sz));
        if (!fdt_allocation) {
            // 两段持久副本必须同时成功，第二次分配失败时回滚第一段。
            memory::buddy()->put_pages(*info_allocation);
            kernel::log::panic("无法分配持久化 FDT");
        }

        const PhyArea info_area(info_allocation->base,
                                info_allocation->base + info_allocation->pages * PAGE_SIZE);
        const PhyArea fdt_area(fdt_allocation->base,
                               fdt_allocation->base + fdt_allocation->pages * PAGE_SIZE);
        if (!memory::page_database().claim(info_area, memory::PageKind::BOOT_DATA, BOOTINFO_OWNER))
        {
            memory::buddy()->put_pages(*fdt_allocation);
            memory::buddy()->put_pages(*info_allocation);
            kernel::log::panic("无法认领持久化 BootInfo 页");
        }
        if (!memory::page_database().claim(fdt_area, memory::PageKind::BOOT_DATA, FDT_OWNER)) {
            memory::page_database().release(info_area, memory::PageKind::BOOT_DATA, BOOTINFO_OWNER);
            memory::buddy()->put_pages(*fdt_allocation);
            memory::buddy()->put_pages(*info_allocation);
            kernel::log::panic("无法认领持久化 FDT 页");
        }

        auto *info = reinterpret_cast<BootInfoHeader *>(PA2KPA(info_allocation->base.arith()));
        auto *fdt  = reinterpret_cast<void *>(PA2KPA(fdt_allocation->base.arith()));
        // 物理页经直映转换为对象存储，复制后 BootInfo 内的 FDT 槽改指向新副本。
        __early_copy(info, &source, source.info_sz);
        __early_copy(fdt, source_fdt, fdt_sz);
        if (fdt_check_full(fdt, fdt_sz) != 0) {
            kernel::log::panic("持久化 FDT 校验失败");
        }
        *bootinfo_fdt_pa(info) = fdt_allocation->base;

        saved_context.info       = info;
        saved_context.fdt        = fdt;
        saved_context.info_paddr = info_allocation->base;
        saved_context.fdt_paddr  = fdt_allocation->base;
        saved_context.info_sz    = source.info_sz;
        saved_context.fdt_sz     = fdt_sz;
        saved_context.info_pages = info_allocation->pages;
        saved_context.fdt_pages  = fdt_allocation->pages;
    }

    size_t reclaim_boot_memory() noexcept {
        if (saved_context.info == nullptr || saved_context.reclaimed) {
            kernel::log::panic("无效的 BootContext 回收状态转换");
        }

        size_t reclaimed = 0;

        const auto *regions = bootinfo_regions(saved_context.info);

        const addr_t init_begin = reinterpret_cast<addr_t>(s_init) - KVA_START;
        const addr_t init_end   = reinterpret_cast<addr_t>(e_init) - KVA_START;
        const PhyArea init_area{PhyAddr(init_begin), PhyAddr(init_end)};

        // 只沿各 FREE parent 的 reservation chain 回收，避免把 parent 本身误当成碎片。
        for (size_t parent_idx = 0; parent_idx < memory::page_database().region_count();
             ++parent_idx)
        {
            size_t child_idx = regions[parent_idx].rsvd_idx;
            while (child_idx != parent_idx) {
                const auto &child = regions[child_idx];
                if (child.type == MemoryType::BOOT_RECLAIMABLE) {
                    if (!is_intersecting(child.area, init_area)) {
                        memory::page_database().release_boot_reclaimable(child.area);
                        memory::buddy()->put_range(child.area);
                        reclaimed += child.area.size() / PAGE_SIZE;
                    } else {
                        const PhyArea diffs[] = {forward_diff(child.area, init_area),
                                                 backward_diff(child.area, init_area)};
                        for (const auto &diff : diffs) {
                            if (diff.nullable())
                                continue;
                            memory::page_database().release_boot_reclaimable(diff);
                            memory::buddy()->put_range(diff);
                            reclaimed += diff.size() / PAGE_SIZE;
                        }
                    }
                }
                const size_t next = child.rsvd_idx;
                if (next == child_idx)
                    break;
                child_idx = next;
            }
        }
        saved_context.reclaimed = true;
        return reclaimed;
    }
}  // namespace boot
