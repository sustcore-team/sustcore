/**
 * @file kernel_mm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 数据驱动的全局内核虚拟内存布局管理器。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <boot/boot.h>
#include <memory/virtual/page_flags.h>
#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/counter.h>
#include <tay/expected.h>
#include <tay/list.h>

#include <atomic>

namespace memory {
    using KernelLayoutId   = u64_t;
    using HHDMLayoutId     = u64_t;
    using ReservedLayoutId = u64_t;

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

    class KernelMM final {
    public:
        [[nodiscard]] static tay::expected<void, tay::error_code> initialize(
            const BootInfoHeader &bootinfo) noexcept;

        KernelMM(const KernelMM &)            = delete;
        KernelMM &operator=(const KernelMM &) = delete;
        KernelMM(KernelMM &&)                 = delete;
        KernelMM &operator=(KernelMM &&)      = delete;

        [[nodiscard]] tay::expected<KernelLayoutId, tay::error_code> load_kernel_layout(
            const KernelLayoutSpec &spec) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> unload_kernel_layout(
            KernelLayoutId id) noexcept;
        [[nodiscard]] tay::expected<HHDMLayoutId, tay::error_code> load_hhdm_layout(
            const HHDMLayout &layout) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> unload_hhdm_layout(
            HHDMLayoutId id) noexcept;
        [[nodiscard]] tay::expected<ReservedLayoutId, tay::error_code> reserve_hhdm(
            HHDMLayoutId parent, const ReservedLayout &layout) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> release_reserved_hhdm(
            ReservedLayoutId id) noexcept;

        [[nodiscard]] tay::expected<void, tay::error_code> unload_kernel_layouts_in(
            KvaAddr begin, size_t bytes) noexcept;
        [[nodiscard]] bool hhdm_covers(PhyAddr begin, size_t bytes) const noexcept;

    private:
        friend KernelMM &kernel_mm() noexcept;

        struct KernelLayoutNode final {
            tay::intrusive_list_hook<KernelLayoutNode *, KernelLayoutNode *> hook{};
            bool committed = false;
            bool runtime   = false;
            KernelLayout layout{};
        };

        using kernel_layout_list = tay::intrusive_list<
            KernelLayoutNode,
            tay::locate_member<KernelLayoutNode,
                               tay::intrusive_list_hook<KernelLayoutNode *, KernelLayoutNode *>,
                               &KernelLayoutNode::hook>>;

        struct ReservedLayoutNode final {
            tay::intrusive_list_hook<ReservedLayoutNode *, ReservedLayoutNode *> hook{};
            bool committed = false;
            bool runtime   = false;
            ReservedLayout layout{};
        };

        using reserved_layout_list = tay::intrusive_list<
            ReservedLayoutNode,
            tay::locate_member<ReservedLayoutNode,
                               tay::intrusive_list_hook<ReservedLayoutNode *, ReservedLayoutNode *>,
                               &ReservedLayoutNode::hook>>;

        struct HHDMLayoutNode final {
            tay::intrusive_list_hook<HHDMLayoutNode *, HHDMLayoutNode *> hook{};
            reserved_layout_list reservations{};
            bool committed = false;
            bool runtime   = false;
            HHDMLayout layout{};
        };

        using hhdm_layout_list = tay::intrusive_list<
            HHDMLayoutNode,
            tay::locate_member<HHDMLayoutNode,
                               tay::intrusive_list_hook<HHDMLayoutNode *, HHDMLayoutNode *>,
                               &HHDMLayoutNode::hook>>;

        struct LayoutState final {
            constexpr LayoutState() noexcept = default;
            kernel_layout_list kernel_layouts{};
            hhdm_layout_list hhdm_layouts{};
            tay::counter<u64_t> ids{1};
        };

        static constexpr size_t BOOTSTRAP_HHDM_NODE_COUNT     = MAX_BOOTINFO_REGIONS + 1;
        static constexpr size_t BOOTSTRAP_KERNEL_NODE_COUNT   = 16;
        static constexpr size_t BOOTSTRAP_RESERVED_NODE_COUNT = BOOTSTRAP_KERNEL_NODE_COUNT;

        constexpr KernelMM() noexcept = default;
        [[nodiscard]] KernelLayoutNode *allocate_kernel_node() noexcept;
        [[nodiscard]] HHDMLayoutNode *allocate_hhdm_node() noexcept;
        [[nodiscard]] ReservedLayoutNode *allocate_reserved_node() noexcept;
        void release_node(KernelLayoutNode *node) noexcept;
        void release_node(HHDMLayoutNode *node) noexcept;
        void release_node(ReservedLayoutNode *node) noexcept;
        [[nodiscard]] tay::expected<HHDMLayoutId, tay::error_code> load_hhdm_layout_impl(
            const HHDMLayout &layout) noexcept;
        [[nodiscard]] tay::expected<KernelLayoutId, tay::error_code> load_kernel_layout_impl(
            const KernelLayoutSpec &spec) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> refresh_hhdm_permissions(
            HHDMLayoutId parent, PhyAddr begin, size_t bytes) noexcept;

        kernel::synchronized<LayoutState> layouts_;
        KernelLayoutNode bootstrap_kernel_nodes_[BOOTSTRAP_KERNEL_NODE_COUNT]{};
        HHDMLayoutNode bootstrap_hhdm_nodes_[BOOTSTRAP_HHDM_NODE_COUNT]{};
        ReservedLayoutNode bootstrap_reserved_nodes_[BOOTSTRAP_RESERVED_NODE_COUNT]{};
        size_t bootstrap_kernel_used_   = 0;
        size_t bootstrap_hhdm_used_     = 0;
        size_t bootstrap_reserved_used_ = 0;
        bool initializing_              = true;
        bool initialization_attempted_  = false;

        static KernelMM instance_;
        static std::atomic<bool> ready_;
    };

    KernelMM &kernel_mm() noexcept;
}  // namespace memory
