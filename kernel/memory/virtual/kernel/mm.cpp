/**
 * @file mm.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KernelMM 布局注册、映射事务及启动布局生成。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/physical/page_db.h>
#include <memory/slab/heap.h>
#include <memory/virtual/kernel/mm.h>
#include <memory/virtual/kernel/vm.h>
#include <memory/virtual/kernel/symbols.h>
#include <sustcore/addrspace.h>

#include <atomic>
#include <cstddef>
#include <new>

namespace memory {
    constinit KernelMM KernelMM::instance_{};
    constinit std::atomic<bool> KernelMM::ready_{false};

    namespace {
        [[nodiscard]] bool valid_range(addr_t begin, size_t bytes) noexcept {
            return bytes != 0 && (begin & (PAGE_SIZE - 1)) == 0 && (bytes & (PAGE_SIZE - 1)) == 0 &&
                   bytes <= addr_t(-1) - begin;
        }

        [[nodiscard]] bool overlaps(addr_t a_begin, size_t a_bytes, addr_t b_begin,
                                    size_t b_bytes) noexcept {
            return a_begin < b_begin + b_bytes && b_begin < a_begin + a_bytes;
        }

        [[nodiscard]] PhyAddr symbol_physical(const char *symbol) noexcept {
            return PhyAddr(reinterpret_cast<addr_t>(symbol) - KVA_START);
        }
    }  // namespace

    template <class Node, size_t N>
    Node *KernelMM::allocate_node(Node (&pool)[N], size_t &used) noexcept {
        if (initializing_) {
            if (used == N)
                return nullptr;
            return &pool[used++];
        }
        auto *node = new (std::nothrow) Node{};
        if (node != nullptr)
            node->runtime = true;
        return node;
    }

    template <class Node>
    void KernelMM::free_runtime_node(Node *node) noexcept {
        if (node != nullptr && node->runtime)
            delete node;
    }

    KernelMM::KernelMapNode *KernelMM::alloc_kernel_node() noexcept {
        return allocate_node(bootstrap_kernel_nodes_, bootstrap_kernel_used_);
    }

    KernelMM::HhdmNode *KernelMM::alloc_hhdm_node() noexcept {
        return allocate_node(bootstrap_hhdm_nodes_, bootstrap_hhdm_used_);
    }

    KernelMM::ResvNode *KernelMM::alloc_resv_node() noexcept {
        return allocate_node(bootstrap_reserved_nodes_, bootstrap_reserved_used_);
    }

    void KernelMM::release_node(KernelMapNode *node) noexcept {
        free_runtime_node(node);
    }

    void KernelMM::release_node(HhdmNode *node) noexcept {
        free_runtime_node(node);
    }

    void KernelMM::release_node(ResvNode *node) noexcept {
        free_runtime_node(node);
    }

    tay::expected<void, KernelMapError> KernelMM::initialize(const BootInfoHeader &) noexcept {
        if (instance_.initialization_attempted_)
            return tay::Err(KernelMapError::InitAlreadyAttempted());
        if (!heap_ready())
            return tay::Err(KernelMapError::DependencyNotReady(KernelMapError::Dependency::HEAP));
        if (!kernel_vm_ready())
            return tay::Err(
                KernelMapError::DependencyNotReady(KernelMapError::Dependency::KERNEL_SPACE));
        instance_.initialization_attempted_ = true;

        for (size_t index = 0; index < page_db().region_count(); ++index) {
            const auto &area = page_db().region(index).parent;
            auto kva         = KpaAddr::try_from(PA2KPA(area.begin.arith()));
            if (!kva)
                return tay::Err(KernelMapError::InvalidHhdmLayout(HHDMLayout{
                    .physical_base = area.begin,
                    .bytes         = area.size(),
                }));
            TAY_TRYV(instance_.map_hhdm_impl(HHDMLayout{
                .virtual_base  = *kva,
                .physical_base = area.begin,
                .bytes         = area.size(),
                .flags         = PageFlags{.readable = true, .writable = true, .executable = false},
            }));
        }

#if defined(__loongarch__)
        constexpr addr_t SERIAL_PAGE = 0x1fe00000;
        TAY_TRYV(instance_.map_hhdm_impl(HHDMLayout{
            .virtual_base  = KpaAddr(PA2KPA(SERIAL_PAGE)),
            .physical_base = PhyAddr(SERIAL_PAGE),
            .bytes         = PAGE_SIZE,
            .flags         = PageFlags{.readable   = true,
                                       .writable   = true,
                                       .executable = false,
                                       .cache      = CacheMode::DEVICE},
        }));
#endif

        const auto load_segment =
            [](char *begin, char *end,
               PageFlags flags) -> tay::expected<KernelLayoutId, KernelMapError> {
            if (begin == end)
                return KernelLayoutId{0};
            return instance_.map_kernel_impl(KernelMapSpec{
                .virtual_base  = KvaAddr(reinterpret_cast<addr_t>(begin)),
                .physical_base = symbol_physical(begin),
                .bytes         = static_cast<size_t>(end - begin),
                .flags         = flags,
            });
        };

        const struct Segment {
            char *begin;
            char *end;
            PageFlags flags;
        } segments[] = {
            {.begin = detail::s_text,
             .end   = detail::e_text,
             .flags = {.readable = true, .executable = true}},
            {.begin = detail::s_rodata, .end = detail::e_rodata, .flags = {.readable = true}},
            {.begin = detail::s_data,
             .end   = detail::e_data,
             .flags = {.readable = true, .writable = true}},
            {.begin = detail::__bsp_stack_bottom,
             .end   = detail::__bsp_stack_top,
             .flags = {.readable = true, .writable = true}},
            {.begin = detail::s_bss,
             .end   = detail::e_bss,
             .flags = {.readable = true, .writable = true}},
            {.begin = detail::s_init_text,
             .end   = detail::e_init_text,
             .flags = {.readable = true, .executable = true}},
            {.begin = detail::s_init_rodata,
             .end   = detail::e_init_rodata,
             .flags = {.readable = true}},
            {.begin = detail::s_init_data,
             .end   = detail::e_init_data,
             .flags = {.readable = true, .writable = true}},
            {.begin = detail::s_init_bss,
             .end   = detail::e_init_bss,
             .flags = {.readable = true, .writable = true}},
        };
        static_assert(sizeof(segments) / sizeof(segments[0]) <= BOOTSTRAP_KERNEL_NODE_COUNT);
        static_assert(sizeof(segments) / sizeof(segments[0]) <= BOOTSTRAP_RESERVED_NODE_COUNT);
        for (const auto &segment : segments)
            TAY_TRYV(load_segment(segment.begin, segment.end, segment.flags));

        // AP 切换最终根后，下一条取指仍使用物理 trampoline PC；只保留该页的低地址 RX
        // 别名，随后汇编立即跳转到已由 s_text..e_text 覆盖的高半区入口。
        auto trampoline =
            kernel_vm().map_trampoline(symbol_physical(detail::s_smp_trampoline));
        if (!trampoline)
            return tay::Err(KernelMapError::PagingFailed(std::move(trampoline.error())));

        instance_.initializing_ = false;
        ready_.store(true, std::memory_order_release);
        return {};
    }

    tay::expected<HHDMLayoutId, KernelMapError> KernelMM::map_hhdm_impl(
        const HHDMLayout &source) noexcept {
        if (!valid_range(source.virtual_base.arith(), source.bytes) ||
            !source.physical_base.aligned<PAGE_SIZE>() ||
            source.bytes > addr_t(-1) - source.physical_base.arith() ||
            source.virtual_base.arith() != PA2KPA(source.physical_base.arith()) ||
            (source.flags.writable && source.flags.executable))
            return tay::Err(KernelMapError::InvalidHhdmLayout(source));

        auto *node = alloc_hhdm_node();
        if (node == nullptr)
            return tay::Err(KernelMapError::NodeAllocationFailed(KernelMapError::LayoutKind::HHDM,
                                                                 initializing_));
        node->layout  = source;
        bool conflict = false;
        PhyArea conflicting_area{};
        {
            auto state = layouts_.lock();
            for (auto *existing : state->hhdm_layouts) {
                if (overlaps(source.physical_base.arith(), source.bytes,
                             existing->layout.physical_base.arith(), existing->layout.bytes))
                {
                    conflict = true;
                    conflicting_area =
                        PhyArea(existing->layout.physical_base,
                                existing->layout.physical_base + existing->layout.bytes);
                    break;
                }
            }
            if (!conflict) {
                node->layout.id = state->ids.next();
                state->hhdm_layouts.push_front(node);
            }
        }
        if (conflict) {
            release_node(node);
            return tay::Err(KernelMapError::HhdmConflict(
                PhyArea(source.physical_base, source.physical_base + source.bytes),
                conflicting_area));
        }

        auto mapped = kernel_vm().map_high(HvaAddr(source.virtual_base.arith()),
                                           source.physical_base, source.bytes, source.flags);
        if (!mapped) {
            {
                auto state = layouts_.lock();
                static_cast<void>(state->hhdm_layouts.remove(node));
            }
            release_node(node);
            return tay::Err(KernelMapError::PagingFailed(std::move(mapped.error())));
        }
        {
            auto state      = layouts_.lock();
            node->committed = true;
        }
        return node->layout.id;
    }

    tay::expected<HHDMLayoutId, KernelMapError> KernelMM::map_hhdm(
        const HHDMLayout &layout) noexcept {
        return map_hhdm_impl(layout);
    }

    bool KernelMM::hhdm_covers(PhyAddr begin, size_t bytes) const noexcept {
        auto state = layouts_.lock();
        for (auto *node : state->hhdm_layouts) {
            if (!node->committed)
                continue;
            if (begin.arith() >= node->layout.physical_base.arith() &&
                begin.arith() + bytes <= node->layout.physical_base.arith() + node->layout.bytes)
                return true;
        }
        return false;
    }

    tay::expected<KernelLayoutId, KernelMapError> KernelMM::map_kernel_impl(
        const KernelMapSpec &spec) noexcept {
        if (!valid_range(spec.virtual_base.arith(), spec.bytes) ||
            !spec.physical_base.aligned<PAGE_SIZE>() ||
            spec.bytes > addr_t(-1) - spec.physical_base.arith() ||
            (spec.flags.writable && spec.flags.executable))
            return tay::Err(KernelMapError::InvalidKernelLayout(spec));
        if (!hhdm_covers(spec.physical_base, spec.bytes))
            return tay::Err(KernelMapError::HhdmCoverageMissing(spec.physical_base, spec.bytes));

        auto *kernel_node   = alloc_kernel_node();
        auto *reserved_node = alloc_resv_node();
        if (kernel_node == nullptr || reserved_node == nullptr) {
            release_node(kernel_node);
            release_node(reserved_node);
            return tay::Err(KernelMapError::NodeAllocationFailed(
                kernel_node == nullptr ? KernelMapError::LayoutKind::KERNEL
                                       : KernelMapError::LayoutKind::RESERVED,
                initializing_));
        }
        kernel_node->layout.spec = spec;

        HhdmNode *parent_node = nullptr;
        bool conflict         = false;
        KvaArea conflicting_area{};
        {
            auto state = layouts_.lock();
            for (auto *existing : state->kernel_layouts) {
                if (overlaps(spec.virtual_base.arith(), spec.bytes,
                             existing->layout.spec.virtual_base.arith(),
                             existing->layout.spec.bytes))
                {
                    conflict = true;
                    conflicting_area =
                        KvaArea(existing->layout.spec.virtual_base,
                                existing->layout.spec.virtual_base + existing->layout.spec.bytes);
                    break;
                }
            }
            for (auto *hhdm : state->hhdm_layouts) {
                if (hhdm->committed &&
                    spec.physical_base.arith() >= hhdm->layout.physical_base.arith() &&
                    spec.physical_base.arith() + spec.bytes <=
                        hhdm->layout.physical_base.arith() + hhdm->layout.bytes)
                {
                    parent_node = hhdm;
                    break;
                }
            }
            if (!conflict && parent_node != nullptr) {
                kernel_node->layout.id = state->ids.next();
                reserved_node->layout  = ReservedLayout{
                     .id            = state->ids.next(),
                     .parent        = parent_node->layout.id,
                     .physical_base = spec.physical_base,
                     .bytes         = spec.bytes,
                     .reason        = ReservedLayout::Reason::KERNEL_LAYOUT,
                     .kernel_owner  = kernel_node->layout.id,
                };
                kernel_node->layout.hhdm_reservation = reserved_node->layout.id;
                state->kernel_layouts.push_front(kernel_node);
                parent_node->reservations.push_front(reserved_node);
            }
        }
        if (conflict || parent_node == nullptr) {
            release_node(kernel_node);
            release_node(reserved_node);
            if (conflict)
                return tay::Err(KernelMapError::LayoutConflict(
                    KvaArea(spec.virtual_base, spec.virtual_base + spec.bytes), conflicting_area));
            return tay::Err(KernelMapError::HhdmCoverageMissing(spec.physical_base, spec.bytes));
        }

        auto mapped = kernel_vm().map_high(HvaAddr(spec.virtual_base.arith()), spec.physical_base,
                                           spec.bytes, spec.flags);
        const bool mapping_created = mapped.has_value();
        if (mapped) {
            mapped = kernel_vm().protect_high(
                HvaAddr(PA2KPA(spec.physical_base.arith())), spec.bytes,
                PageFlags{.readable = true, .writable = false, .executable = false});
        }
        if (!mapped) {
            if (mapping_created) {
                auto rolled_back =
                    kernel_vm().unmap_high(HvaAddr(spec.virtual_base.arith()), spec.bytes);
                if (!rolled_back)
                    kernel::log::panic("KernelLayout 映射事务回滚失败");
            }
            {
                auto state = layouts_.lock();
                static_cast<void>(state->kernel_layouts.remove(kernel_node));
                static_cast<void>(parent_node->reservations.remove(reserved_node));
            }
            release_node(kernel_node);
            release_node(reserved_node);
            return tay::Err(KernelMapError::PagingFailed(std::move(mapped.error())));
        }
        {
            auto state               = layouts_.lock();
            kernel_node->committed   = true;
            reserved_node->committed = true;
        }
        return kernel_node->layout.id;
    }

    tay::expected<KernelLayoutId, KernelMapError> KernelMM::map_kernel(
        const KernelMapSpec &spec) noexcept {
        return map_kernel_impl(spec);
    }

    tay::expected<ResvId, KernelMapError> KernelMM::reserve_hhdm(
        HHDMLayoutId parent, const ReservedLayout &source) noexcept {
        if (!valid_range(source.physical_base.arith(), source.bytes) ||
            source.bytes > addr_t(-1) - source.physical_base.arith())
            return tay::Err(KernelMapError::InvalidReservedLayout(source));
        auto *node = alloc_resv_node();
        if (node == nullptr)
            return tay::Err(KernelMapError::NodeAllocationFailed(
                KernelMapError::LayoutKind::RESERVED, initializing_));
        node->layout          = source;
        node->layout.parent   = parent;
        HhdmNode *parent_node = nullptr;
        {
            auto state = layouts_.lock();
            for (auto *hhdm : state->hhdm_layouts) {
                if (hhdm->committed && hhdm->layout.id == parent) {
                    parent_node = hhdm;
                    break;
                }
            }
            if (parent_node != nullptr &&
                source.physical_base.arith() >= parent_node->layout.physical_base.arith() &&
                source.physical_base.arith() + source.bytes <=
                    parent_node->layout.physical_base.arith() + parent_node->layout.bytes)
            {
                node->layout.id = state->ids.next();
                parent_node->reservations.push_front(node);
            }
        }
        if (parent_node == nullptr) {
            release_node(node);
            return tay::Err(KernelMapError::HhdmLayoutNotFound(parent));
        }
        if (node->layout.id == 0) {
            release_node(node);
            return tay::Err(
                KernelMapError::HhdmCoverageMissing(source.physical_base, source.bytes));
        }
        auto protected_range = kernel_vm().protect_high(
            HvaAddr(PA2KPA(source.physical_base.arith())), source.bytes,
            PageFlags{.readable = true, .writable = false, .executable = false});
        if (!protected_range) {
            {
                auto state = layouts_.lock();
                static_cast<void>(parent_node->reservations.remove(node));
            }
            release_node(node);
            return tay::Err(KernelMapError::PagingFailed(std::move(protected_range.error())));
        }
        {
            auto state      = layouts_.lock();
            node->committed = true;
        }
        return node->layout.id;
    }

    tay::expected<void, KernelMapError> KernelMM::refresh_hhdm_perms(HHDMLayoutId parent,
                                                                     PhyAddr begin,
                                                                     size_t bytes) noexcept {
        for (size_t offset = 0; offset < bytes; offset += PAGE_SIZE) {
            bool reserved = false;
            PageFlags flags{};
            bool found_parent = false;
            {
                auto state = layouts_.lock();
                for (auto *hhdm : state->hhdm_layouts) {
                    if (hhdm->layout.id != parent)
                        continue;
                    flags        = hhdm->layout.flags;
                    found_parent = true;
                    for (auto *reservation : hhdm->reservations) {
                        if (!reservation->committed)
                            continue;
                        const addr_t page = begin.arith() + offset;
                        if (page >= reservation->layout.physical_base.arith() &&
                            page < reservation->layout.physical_base.arith() +
                                       reservation->layout.bytes)
                        {
                            reserved = true;
                            break;
                        }
                    }
                    break;
                }
            }
            if (!found_parent)
                return tay::Err(KernelMapError::HhdmLayoutNotFound(parent));
            flags.writable   = flags.writable && !reserved;
            flags.executable = false;
            auto result =
                kernel_vm().protect_high(HvaAddr(PA2KPA(begin.arith() + offset)), PAGE_SIZE, flags);
            if (!result)
                return tay::Err(KernelMapError::PagingFailed(std::move(result.error())));
        }
        return {};
    }

    tay::expected<void, KernelMapError> KernelMM::release_hhdm_resv(ResvId id) noexcept {
        HhdmNode *parent_node = nullptr;
        ResvNode *target      = nullptr;
        {
            auto state = layouts_.lock();
            for (auto *hhdm : state->hhdm_layouts) {
                for (auto *reservation : hhdm->reservations) {
                    if (reservation->layout.id == id) {
                        parent_node = hhdm;
                        target      = reservation;
                        break;
                    }
                }
                if (target != nullptr)
                    break;
            }
            if (target == nullptr)
                return tay::Err(KernelMapError::ReservedLayoutNotFound(id));
            if (target->layout.reason == ReservedLayout::Reason::KERNEL_LAYOUT)
                return tay::Err(
                    KernelMapError::ReservationOwnedByKernel(id, target->layout.kernel_owner));
            target->committed = false;
        }
        auto result = refresh_hhdm_perms(target->layout.parent, target->layout.physical_base,
                                         target->layout.bytes);
        if (!result) {
            auto state        = layouts_.lock();
            target->committed = true;
            return TAY_ERR(result);
        }
        {
            auto state = layouts_.lock();
            static_cast<void>(parent_node->reservations.remove(target));
        }
        release_node(target);
        return {};
    }

    tay::expected<void, KernelMapError> KernelMM::unmap_kernel(KernelLayoutId id) noexcept {
        KernelMapNode *kernel_node = nullptr;
        HhdmNode *parent_node      = nullptr;
        ResvNode *reserved_node    = nullptr;
        {
            auto state = layouts_.lock();
            for (auto *node : state->kernel_layouts) {
                if (node->committed && node->layout.id == id) {
                    kernel_node = node;
                    break;
                }
            }
            for (auto *hhdm : state->hhdm_layouts) {
                for (auto *reservation : hhdm->reservations) {
                    if (reservation->committed && reservation->layout.kernel_owner == id) {
                        parent_node   = hhdm;
                        reserved_node = reservation;
                        break;
                    }
                }
                if (reserved_node != nullptr)
                    break;
            }
            if (kernel_node == nullptr)
                return tay::Err(KernelMapError::KernelLayoutNotFound(id));
            if (parent_node == nullptr || reserved_node == nullptr)
                return tay::Err(KernelMapError::OwnershipMismatch(id));
            kernel_node->committed   = false;
            reserved_node->committed = false;
        }

        auto unmapped = kernel_vm().unmap_high(
            HvaAddr(kernel_node->layout.spec.virtual_base.arith()), kernel_node->layout.spec.bytes);
        if (!unmapped) {
            auto state               = layouts_.lock();
            kernel_node->committed   = true;
            reserved_node->committed = true;
            return tay::Err(KernelMapError::PagingFailed(std::move(unmapped.error())));
        }
        auto refreshed =
            refresh_hhdm_perms(reserved_node->layout.parent, reserved_node->layout.physical_base,
                               reserved_node->layout.bytes);
        if (!refreshed)
            kernel::log::panic("卸载 KernelLayout 后无法恢复 HHDM 权限");

        {
            auto state = layouts_.lock();
            static_cast<void>(state->kernel_layouts.remove(kernel_node));
            static_cast<void>(parent_node->reservations.remove(reserved_node));
        }
        release_node(kernel_node);
        release_node(reserved_node);
        return {};
    }

    tay::expected<void, KernelMapError> KernelMM::unmap_hhdm(HHDMLayoutId id) noexcept {
        HhdmNode *target = nullptr;
        {
            auto state = layouts_.lock();
            for (auto *node : state->hhdm_layouts) {
                if (node->layout.id == id) {
                    target = node;
                    break;
                }
            }
            if (target == nullptr)
                return tay::Err(KernelMapError::HhdmLayoutNotFound(id));
            if (!target->reservations.empty())
                return tay::Err(KernelMapError::ReservationsPresent(id));
            target->committed = false;
        }
        auto result = kernel_vm().unmap_high(HvaAddr(target->layout.virtual_base.arith()),
                                             target->layout.bytes);
        if (!result) {
            auto state        = layouts_.lock();
            target->committed = true;
            return tay::Err(KernelMapError::PagingFailed(std::move(result.error())));
        }
        {
            auto state = layouts_.lock();
            static_cast<void>(state->hhdm_layouts.remove(target));
        }
        release_node(target);
        return {};
    }

    tay::expected<void, KernelMapError> KernelMM::unmap_kernel_in(KvaAddr begin,
                                                                  size_t bytes) noexcept {
        if (!valid_range(begin.arith(), bytes))
            return tay::Err(KernelMapError::InvalidKernelLayout(KernelMapSpec{
                .virtual_base = begin,
                .bytes        = bytes,
            }));
        while (true) {
            KernelLayoutId found = 0;
            {
                auto state = layouts_.lock();
                for (auto *node : state->kernel_layouts) {
                    if (!node->committed)
                        continue;
                    if (node->layout.spec.virtual_base.arith() >= begin.arith() &&
                        node->layout.spec.virtual_base.arith() + node->layout.spec.bytes <=
                            begin.arith() + bytes)
                    {
                        found = node->layout.id;
                        break;
                    }
                }
            }
            if (found == 0)
                return {};
            TAY_TRYV(unmap_kernel(found));
        }
    }

    KernelMM &kernel_mm() noexcept {
        auto *manager =
            KernelMM::ready_.load(std::memory_order_acquire) ? &KernelMM::instance_ : nullptr;
        if (manager == nullptr)
            kernel::log::panic("KernelMM 尚未初始化");
        return *manager;
    }
}  // namespace memory
