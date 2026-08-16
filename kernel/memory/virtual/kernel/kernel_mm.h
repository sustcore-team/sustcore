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
#include <memory/virtual/kernel/kernel_layout.h>
#include <memory/virtual/kernel/kernel_layout_error.h>
#include <synchronized.h>
#include <tay/counter.h>
#include <tay/expected.h>
#include <tay/list.h>

#include <atomic>

namespace memory {
    class KernelMM final {
    public:
        [[nodiscard]] static tay::expected<void, KernelLayoutError> initialize(
            const BootInfoHeader &bootinfo) noexcept;

        KernelMM(const KernelMM &)            = delete;
        KernelMM &operator=(const KernelMM &) = delete;
        KernelMM(KernelMM &&)                 = delete;
        KernelMM &operator=(KernelMM &&)      = delete;

        [[nodiscard]] tay::expected<KernelLayoutId, KernelLayoutError> load_kernel_layout(
            const KernelLayoutSpec &spec) noexcept;
        [[nodiscard]] tay::expected<void, KernelLayoutError> unload_kernel_layout(
            KernelLayoutId id) noexcept;
        [[nodiscard]] tay::expected<HHDMLayoutId, KernelLayoutError> load_hhdm_layout(
            const HHDMLayout &layout) noexcept;
        [[nodiscard]] tay::expected<void, KernelLayoutError> unload_hhdm_layout(
            HHDMLayoutId id) noexcept;
        [[nodiscard]] tay::expected<ReservedLayoutId, KernelLayoutError> reserve_hhdm(
            HHDMLayoutId parent, const ReservedLayout &layout) noexcept;
        [[nodiscard]] tay::expected<void, KernelLayoutError> release_reserved_hhdm(
            ReservedLayoutId id) noexcept;

        [[nodiscard]] tay::expected<void, KernelLayoutError> unload_kernel_layouts_in(
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
        [[nodiscard]] tay::expected<HHDMLayoutId, KernelLayoutError> load_hhdm_layout_impl(
            const HHDMLayout &layout) noexcept;
        [[nodiscard]] tay::expected<KernelLayoutId, KernelLayoutError> load_kernel_layout_impl(
            const KernelLayoutSpec &spec) noexcept;
        [[nodiscard]] tay::expected<void, KernelLayoutError> refresh_hhdm_permissions(
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
