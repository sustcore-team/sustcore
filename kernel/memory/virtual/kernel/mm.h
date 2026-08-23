/**
 * @file mm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 数据驱动的全局内核虚拟内存布局管理器。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <boot/boot.h>
#include <memory/virtual/kernel/map.h>
#include <error/kernel_map.h>
#include <synchronized.h>
#include <tay/counter.h>
#include <tay/expected.h>
#include <tay/list.h>

#include <atomic>

namespace memory {
    class KernelMM final {
    public:
        [[nodiscard]] static tay::expected<void, KernelMapError> initialize(
            const BootInfoHeader &bootinfo) noexcept;

        KernelMM(const KernelMM &)            = delete;
        KernelMM &operator=(const KernelMM &) = delete;
        KernelMM(KernelMM &&)                 = delete;
        KernelMM &operator=(KernelMM &&)      = delete;

        [[nodiscard]] tay::expected<KernelLayoutId, KernelMapError> map_kernel(
            const KernelMapSpec &spec) noexcept;
        [[nodiscard]] tay::expected<void, KernelMapError> unmap_kernel(KernelLayoutId id) noexcept;
        [[nodiscard]] tay::expected<HHDMLayoutId, KernelMapError> map_hhdm(
            const HHDMLayout &layout) noexcept;
        [[nodiscard]] tay::expected<void, KernelMapError> unmap_hhdm(HHDMLayoutId id) noexcept;
        [[nodiscard]] tay::expected<ResvId, KernelMapError> reserve_hhdm(
            HHDMLayoutId parent, const ReservedLayout &layout) noexcept;
        [[nodiscard]] tay::expected<void, KernelMapError> release_hhdm_resv(ResvId id) noexcept;

        [[nodiscard]] tay::expected<void, KernelMapError> unmap_kernel_in(KvaAddr begin,
                                                                          size_t bytes) noexcept;
        [[nodiscard]] bool hhdm_covers(PhyAddr begin, size_t bytes) const noexcept;

    private:
        friend KernelMM &kernel_mm() noexcept;

        struct KernelMapNode final {
            tay::intrusive_list_hook<KernelMapNode *, KernelMapNode *> hook{};
            bool committed = false;
            bool runtime   = false;
            KernelLayout layout{};
        };

        using kernel_layout_list = tay::intrusive_list<
            KernelMapNode,
            tay::locate_member<KernelMapNode,
                               tay::intrusive_list_hook<KernelMapNode *, KernelMapNode *>,
                               &KernelMapNode::hook>>;

        struct ResvNode final {
            tay::intrusive_list_hook<ResvNode *, ResvNode *> hook{};
            bool committed = false;
            bool runtime   = false;
            ReservedLayout layout{};
        };

        using reserved_layout_list = tay::intrusive_list<
            ResvNode, tay::locate_member<ResvNode, tay::intrusive_list_hook<ResvNode *, ResvNode *>,
                                         &ResvNode::hook>>;

        struct HhdmNode final {
            tay::intrusive_list_hook<HhdmNode *, HhdmNode *> hook{};
            reserved_layout_list reservations{};
            bool committed = false;
            bool runtime   = false;
            HHDMLayout layout{};
        };

        using hhdm_layout_list = tay::intrusive_list<
            HhdmNode, tay::locate_member<HhdmNode, tay::intrusive_list_hook<HhdmNode *, HhdmNode *>,
                                         &HhdmNode::hook>>;

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
        template <class Node, size_t N>
        [[nodiscard]] Node *allocate_node(Node (&pool)[N], size_t &used) noexcept;
        template <class Node>
        void free_runtime_node(Node *node) noexcept;
        [[nodiscard]] KernelMapNode *alloc_kernel_node() noexcept;
        [[nodiscard]] HhdmNode *alloc_hhdm_node() noexcept;
        [[nodiscard]] ResvNode *alloc_resv_node() noexcept;
        void release_node(KernelMapNode *node) noexcept;
        void release_node(HhdmNode *node) noexcept;
        void release_node(ResvNode *node) noexcept;
        [[nodiscard]] tay::expected<HHDMLayoutId, KernelMapError> map_hhdm_impl(
            const HHDMLayout &layout) noexcept;
        [[nodiscard]] tay::expected<KernelLayoutId, KernelMapError> map_kernel_impl(
            const KernelMapSpec &spec) noexcept;
        [[nodiscard]] tay::expected<void, KernelMapError> refresh_hhdm_perms(HHDMLayoutId parent,
                                                                             PhyAddr begin,
                                                                             size_t bytes) noexcept;

        kernel::synchronized<LayoutState> layouts_;
        KernelMapNode bootstrap_kernel_nodes_[BOOTSTRAP_KERNEL_NODE_COUNT]{};
        HhdmNode bootstrap_hhdm_nodes_[BOOTSTRAP_HHDM_NODE_COUNT]{};
        ResvNode bootstrap_reserved_nodes_[BOOTSTRAP_RESERVED_NODE_COUNT]{};
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
