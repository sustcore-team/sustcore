/**
 * @file vm_priv.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 最终内核地址空间的分段建造接口
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <boot/boot.h>
#include <memory/virtual/kernel/vm.h>

namespace memory::detail {
    [[nodiscard]] PhyAddr symbol_paddr(const char *symbol) noexcept;
    [[nodiscard]] PhyAddr kernel_start_paddr() noexcept;
    [[nodiscard]] PhyAddr kernel_end_paddr() noexcept;
    [[nodiscard]] tay::expected<void, tay::error_code> map_kernel_layout(KernelVm &space) noexcept;
    [[nodiscard]] tay::expected<void, tay::error_code> map_direct_regions(
        KernelVm &space, const BootInfoHeader &bootinfo) noexcept;
}  // namespace memory::detail
