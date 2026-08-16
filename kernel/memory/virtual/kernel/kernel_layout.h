/**
 * @file kernel_layout.h
 * @brief 定义 KernelMM 管理的稳定布局值与标识符。
 */

#pragma once

#include <memory/virtual/page_flags.h>
#include <sustcore/addr.h>
#include <tay/bits.h>
#include <tay/range.h>

#include <cstddef>

namespace memory {
    using KernelLayoutId   = u64_t;
    using HHDMLayoutId     = u64_t;
    using ReservedLayoutId = u64_t;

    using KvaArea = tay::range<KvaAddr>;
    using HvaArea = tay::range<HvaAddr>;

    struct KernelLayoutSpec final {
        KvaAddr virtual_base{};
        PhyAddr physical_base{};
        size_t bytes = 0;
        PageFlags flags{};
    };

    struct KernelLayout final {
        KernelLayoutId id = 0;
        KernelLayoutSpec spec{};
        ReservedLayoutId hhdm_reservation = 0;
    };

    struct HHDMLayout final {
        HHDMLayoutId id = 0;
        KpaAddr virtual_base{};
        PhyAddr physical_base{};
        size_t bytes = 0;
        PageFlags flags{.readable = true, .writable = true, .executable = false};
    };

    struct ReservedLayout final {
        enum class Reason : u8_t {
            KERNEL_LAYOUT,
            FIRMWARE,
            DEVICE,
            OTHER,
        };

        ReservedLayoutId id = 0;
        HHDMLayoutId parent = 0;
        PhyAddr physical_base{};
        size_t bytes                = 0;
        Reason reason               = Reason::OTHER;
        KernelLayoutId kernel_owner = 0;
    };
}  // namespace memory
