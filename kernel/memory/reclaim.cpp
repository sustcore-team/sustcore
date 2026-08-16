/**
 * @file reclaim.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核初始化段的别名切换与物理页回收
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/physical/page_database.h>
#include <memory/reclaim.h>
#include <memory/virtual/kernel/kernel_mm.h>
#include <memory/virtual/kernel/symbols.h>
#include <sustcore/addrspace.h>

namespace memory {
    size_t reclaim_init_memory() noexcept {
        const addr_t init_vbegin = reinterpret_cast<addr_t>(detail::s_init);
        const addr_t init_vend   = reinterpret_cast<addr_t>(detail::e_init);
        const addr_t init_pbegin = init_vbegin - KVA_START;
        const size_t bytes       = init_vend - init_vbegin;
        const PhyArea init_area(PhyAddr(init_pbegin), PhyAddr(init_pbegin + bytes));

        auto result = kernel_mm().unload_kernel_layouts_in(KvaAddr(init_vbegin), bytes);
        if (!result)
            kernel::log::panic("无法通过 KernelMM 卸载 .init 布局: {}", result.error());

        page_database().release_boot_reclaimable(init_area);
        buddy()->put_range(init_area);
        return bytes / PAGE_SIZE;
    }
}  // namespace memory
