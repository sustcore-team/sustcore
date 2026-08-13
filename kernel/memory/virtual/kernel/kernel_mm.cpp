/**
 * @file kernel_mm.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KernelMM 布局注册、映射事务及启动布局生成。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/physical/page_database.h>
#include <memory/slab/heap.h>
#include <memory/virtual/kernel/kernel_mm.h>
#include <memory/virtual/kernel/kernel_space.h>
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

    KernelMM::KernelLayoutNode *KernelMM::allocate_kernel_node() noexcept {
        if (initializing_) {
            if (bootstrap_kernel_used_ == BOOTSTRAP_KERNEL_NODE_COUNT)
                return nullptr;
            return &bootstrap_kernel_nodes_[bootstrap_kernel_used_++];
        }
        auto *node = new (std::nothrow) KernelLayoutNode{};
        if (node != nullptr)
            node->runtime = true;
        return node;
    }

    KernelMM::HHDMLayoutNode *KernelMM::allocate_hhdm_node() noexcept {
        if (initializing_) {
            if (bootstrap_hhdm_used_ == BOOTSTRAP_HHDM_NODE_COUNT)
                return nullptr;
            return &bootstrap_hhdm_nodes_[bootstrap_hhdm_used_++];
        }
        auto *node = new (std::nothrow) HHDMLayoutNode{};
        if (node != nullptr)
            node->runtime = true;
        return node;
    }

    KernelMM::ReservedLayoutNode *KernelMM::allocate_reserved_node() noexcept {
        if (initializing_) {
            if (bootstrap_reserved_used_ == BOOTSTRAP_RESERVED_NODE_COUNT)
                return nullptr;
            return &bootstrap_reserved_nodes_[bootstrap_reserved_used_++];
        }
        auto *node = new (std::nothrow) ReservedLayoutNode{};
        if (node != nullptr)
            node->runtime = true;
        return node;
    }

    void KernelMM::release_node(KernelLayoutNode *node) noexcept {
        if (node != nullptr && node->runtime)
            delete node;
    }

    void KernelMM::release_node(HHDMLayoutNode *node) noexcept {
        if (node != nullptr && node->runtime)
            delete node;
    }

    void KernelMM::release_node(ReservedLayoutNode *node) noexcept {
        if (node != nullptr && node->runtime)
            delete node;
    }

    tay::expected<void, tay::error_code> KernelMM::initialize(const BootInfoHeader &) noexcept {
        if (instance_.initialization_attempted_ || !heap_ready() || !kernel_space_ready())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        instance_.initialization_attempted_ = true;

        for (size_t index = 0; index < page_database().region_count(); ++index) {
            const auto &area = page_database().region(index).parent;
            auto kva         = KpaAddr::try_from(PA2KPA(area.begin.arith()));
            if (!kva)
                return tay::Err(kva.error());
            auto loaded = instance_.load_hhdm_layout_impl(HHDMLayout{
                .virtual_base  = *kva,
                .physical_base = area.begin,
                .bytes         = area.size(),
                .flags         = PageFlags{.readable = true, .writable = true, .executable = false},
            });
            if (!loaded)
                return tay::Err(loaded.error());
        }

#if defined(__loongarch__)
        constexpr addr_t SERIAL_PAGE = 0x1fe00000;
        auto serial                  = instance_.load_hhdm_layout_impl(HHDMLayout{
                             .virtual_base  = KpaAddr(PA2KPA(SERIAL_PAGE)),
                             .physical_base = PhyAddr(SERIAL_PAGE),
                             .bytes         = PAGE_SIZE,
                             .flags         = PageFlags{.readable   = true,
                                                        .writable   = true,
                                                        .executable = false,
                                                        .cache      = CacheMode::DEVICE},
        });
        if (!serial)
            return tay::Err(serial.error());
#endif

        const auto load_segment =
            [](char *begin, char *end,
               PageFlags flags) -> tay::expected<KernelLayoutId, tay::error_code> {
            if (begin == end)
                return KernelLayoutId{0};
            return instance_.load_kernel_layout_impl(KernelLayoutSpec{
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
            {detail::s_text, detail::e_text, {.readable = true, .executable = true}},
            {detail::s_rodata, detail::e_rodata, {.readable = true}},
            {detail::s_data, detail::e_data, {.readable = true, .writable = true}},
            {detail::__bsp_stack_bottom,
             detail::__bsp_stack_top,
             {.readable = true, .writable = true}},
            {detail::s_bss, detail::e_bss, {.readable = true, .writable = true}},
            {detail::s_init_text, detail::e_init_text, {.readable = true, .executable = true}},
            {detail::s_init_rodata, detail::e_init_rodata, {.readable = true}},
            {detail::s_init_data, detail::e_init_data, {.readable = true, .writable = true}},
            {detail::s_init_bss, detail::e_init_bss, {.readable = true, .writable = true}},
        };
        static_assert(sizeof(segments) / sizeof(segments[0]) <= BOOTSTRAP_KERNEL_NODE_COUNT);
        static_assert(sizeof(segments) / sizeof(segments[0]) <= BOOTSTRAP_RESERVED_NODE_COUNT);
        for (const auto &segment : segments) {
            auto loaded = load_segment(segment.begin, segment.end, segment.flags);
            if (!loaded)
                return tay::Err(loaded.error());
        }

        instance_.initializing_ = false;
        ready_.store(true, std::memory_order_release);
        return {};
    }

    tay::expected<HHDMLayoutId, tay::error_code> KernelMM::load_hhdm_layout_impl(
        const HHDMLayout &source) noexcept {
        if (!valid_range(source.virtual_base.arith(), source.bytes) ||
            !source.physical_base.aligned<PAGE_SIZE>() ||
            source.bytes > addr_t(-1) - source.physical_base.arith() ||
            source.virtual_base.arith() != PA2KPA(source.physical_base.arith()) ||
            (source.flags.writable && source.flags.executable))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto *node = allocate_hhdm_node();
        if (node == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        node->layout  = source;
        bool conflict = false;
        {
            auto state = layouts_.lock();
            for (auto *existing : state->hhdm_layouts) {
                if (overlaps(source.physical_base.arith(), source.bytes,
                             existing->layout.physical_base.arith(), existing->layout.bytes))
                {
                    conflict = true;
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
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }

        auto mapped = kernel_space().map_high(HvaAddr(source.virtual_base.arith()),
                                              source.physical_base, source.bytes, source.flags);
        if (!mapped) {
            {
                auto state = layouts_.lock();
                static_cast<void>(state->hhdm_layouts.remove(node));
            }
            release_node(node);
            return tay::Err(mapped.error());
        }
        {
            auto state      = layouts_.lock();
            node->committed = true;
        }
        return node->layout.id;
    }

    tay::expected<HHDMLayoutId, tay::error_code> KernelMM::load_hhdm_layout(
        const HHDMLayout &layout) noexcept {
        return load_hhdm_layout_impl(layout);
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

    tay::expected<KernelLayoutId, tay::error_code> KernelMM::load_kernel_layout_impl(
        const KernelLayoutSpec &spec) noexcept {
        if (!valid_range(spec.virtual_base.arith(), spec.bytes) ||
            !spec.physical_base.aligned<PAGE_SIZE>() ||
            spec.bytes > addr_t(-1) - spec.physical_base.arith() ||
            !hhdm_covers(spec.physical_base, spec.bytes) ||
            (spec.flags.writable && spec.flags.executable))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto *kernel_node   = allocate_kernel_node();
        auto *reserved_node = allocate_reserved_node();
        if (kernel_node == nullptr || reserved_node == nullptr) {
            release_node(kernel_node);
            release_node(reserved_node);
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        }
        kernel_node->layout.spec = spec;

        HHDMLayoutNode *parent_node = nullptr;
        bool conflict               = false;
        {
            auto state = layouts_.lock();
            for (auto *existing : state->kernel_layouts) {
                if (overlaps(spec.virtual_base.arith(), spec.bytes,
                             existing->layout.spec.virtual_base.arith(),
                             existing->layout.spec.bytes))
                {
                    conflict = true;
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
            return tay::Err(conflict ? tay::error_code::INVALID_ARGUMENT
                                     : tay::error_code::OUT_OF_RANGE);
        }

        auto mapped = kernel_space().map_high(HvaAddr(spec.virtual_base.arith()),
                                              spec.physical_base, spec.bytes, spec.flags);
        if (mapped) {
            mapped = kernel_space().protect_high(
                HvaAddr(PA2KPA(spec.physical_base.arith())), spec.bytes,
                PageFlags{.readable = true, .writable = false, .executable = false});
        }
        if (!mapped) {
            static_cast<void>(
                kernel_space().unmap_high(HvaAddr(spec.virtual_base.arith()), spec.bytes));
            {
                auto state = layouts_.lock();
                static_cast<void>(state->kernel_layouts.remove(kernel_node));
                static_cast<void>(parent_node->reservations.remove(reserved_node));
            }
            release_node(kernel_node);
            release_node(reserved_node);
            return tay::Err(mapped.error());
        }
        {
            auto state               = layouts_.lock();
            kernel_node->committed   = true;
            reserved_node->committed = true;
        }
        return kernel_node->layout.id;
    }

    tay::expected<KernelLayoutId, tay::error_code> KernelMM::load_kernel_layout(
        const KernelLayoutSpec &spec) noexcept {
        return load_kernel_layout_impl(spec);
    }

    tay::expected<ReservedLayoutId, tay::error_code> KernelMM::reserve_hhdm(
        HHDMLayoutId parent, const ReservedLayout &source) noexcept {
        if (!valid_range(source.physical_base.arith(), source.bytes) ||
            source.bytes > addr_t(-1) - source.physical_base.arith())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto *node = allocate_reserved_node();
        if (node == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        node->layout                = source;
        node->layout.parent         = parent;
        HHDMLayoutNode *parent_node = nullptr;
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
            } else {
                parent_node = nullptr;
            }
        }
        if (parent_node == nullptr) {
            release_node(node);
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        }
        auto protected_range = kernel_space().protect_high(
            HvaAddr(PA2KPA(source.physical_base.arith())), source.bytes,
            PageFlags{.readable = true, .writable = false, .executable = false});
        if (!protected_range) {
            {
                auto state = layouts_.lock();
                static_cast<void>(parent_node->reservations.remove(node));
            }
            release_node(node);
            return tay::Err(protected_range.error());
        }
        {
            auto state      = layouts_.lock();
            node->committed = true;
        }
        return node->layout.id;
    }

    tay::expected<void, tay::error_code> KernelMM::refresh_hhdm_permissions(HHDMLayoutId parent,
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
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            flags.writable   = flags.writable && !reserved;
            flags.executable = false;
            auto result      = kernel_space().protect_high(HvaAddr(PA2KPA(begin.arith() + offset)),
                                                           PAGE_SIZE, flags);
            if (!result)
                return result;
        }
        return {};
    }

    tay::expected<void, tay::error_code> KernelMM::release_reserved_hhdm(
        ReservedLayoutId id) noexcept {
        HHDMLayoutNode *parent_node = nullptr;
        ReservedLayoutNode *target  = nullptr;
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
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            if (target->layout.reason == ReservedLayout::Reason::KERNEL_LAYOUT)
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            target->committed = false;
        }
        auto result = refresh_hhdm_permissions(target->layout.parent, target->layout.physical_base,
                                               target->layout.bytes);
        if (!result) {
            auto state        = layouts_.lock();
            target->committed = true;
            return result;
        }
        {
            auto state = layouts_.lock();
            static_cast<void>(parent_node->reservations.remove(target));
        }
        release_node(target);
        return {};
    }

    tay::expected<void, tay::error_code> KernelMM::unload_kernel_layout(
        KernelLayoutId id) noexcept {
        KernelLayoutNode *kernel_node     = nullptr;
        HHDMLayoutNode *parent_node       = nullptr;
        ReservedLayoutNode *reserved_node = nullptr;
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
            if (kernel_node == nullptr || parent_node == nullptr || reserved_node == nullptr)
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            kernel_node->committed   = false;
            reserved_node->committed = false;
        }

        auto unmapped = kernel_space().unmap_high(
            HvaAddr(kernel_node->layout.spec.virtual_base.arith()), kernel_node->layout.spec.bytes);
        if (!unmapped) {
            auto state               = layouts_.lock();
            kernel_node->committed   = true;
            reserved_node->committed = true;
            return unmapped;
        }
        auto refreshed = refresh_hhdm_permissions(reserved_node->layout.parent,
                                                  reserved_node->layout.physical_base,
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

    tay::expected<void, tay::error_code> KernelMM::unload_hhdm_layout(HHDMLayoutId id) noexcept {
        HHDMLayoutNode *target = nullptr;
        {
            auto state = layouts_.lock();
            for (auto *node : state->hhdm_layouts) {
                if (node->layout.id == id) {
                    target = node;
                    break;
                }
            }
            if (target == nullptr)
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            if (!target->reservations.empty())
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            target->committed = false;
        }
        auto result = kernel_space().unmap_high(HvaAddr(target->layout.virtual_base.arith()),
                                                target->layout.bytes);
        if (!result) {
            auto state        = layouts_.lock();
            target->committed = true;
            return result;
        }
        {
            auto state = layouts_.lock();
            static_cast<void>(state->hhdm_layouts.remove(target));
        }
        release_node(target);
        return {};
    }

    tay::expected<void, tay::error_code> KernelMM::unload_kernel_layouts_in(KvaAddr begin,
                                                                            size_t bytes) noexcept {
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
            auto result = unload_kernel_layout(found);
            if (!result)
                return result;
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
