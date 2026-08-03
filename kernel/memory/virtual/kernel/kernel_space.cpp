/**
 * @file kernel_space.cpp
 * @brief 全局 KernelSpace 单例及其受锁保护的页表执行接口。
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/physical/page_database.h>
#include <memory/virtual/kernel/kernel_space.h>

#include <cstddef>

namespace memory {
    constinit KernelSpace KernelSpace::instance_{};
    constinit std::atomic<bool> KernelSpace::ready_{false};

    tay::expected<void, tay::error_code> KernelSpace::initialize(const BootInfoHeader &) noexcept {
        if (ready_.load(std::memory_order_acquire))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto root = PageTable::create_root(KERNEL_PAGE_TABLE_OWNER);
        if (!root)
            return tay::Err(root.error());
        auto guard = PageTable::create_root(KERNEL_PAGE_TABLE_OWNER);
        if (!guard) {
            PageTable::destroy_root(*root, KERNEL_PAGE_TABLE_OWNER);
            return tay::Err(guard.error());
        }

        {
            auto state = instance_.page_table_.lock();
            auto adopted =
                state->table.adopt_root(*root, PageTableKind::KERNEL, KERNEL_PAGE_TABLE_OWNER);
            if (!adopted) {
                PageTable::destroy_root(*guard, KERNEL_PAGE_TABLE_OWNER);
                PageTable::destroy_root(*root, KERNEL_PAGE_TABLE_OWNER);
                return tay::Err(adopted.error());
            }
            instance_.guard_root_ = *guard;
        }
        ready_.store(true, std::memory_order_release);
        return {};
    }

    tay::expected<void, tay::error_code> KernelSpace::map_high(HvaAddr address, PhyAddr physical,
                                                               size_t bytes,
                                                               PageFlags flags) noexcept {
        flags.user   = false;
        flags.global = true;
        auto state   = page_table_.lock();
        return state->table.map(address.arith(), physical, bytes, flags,
                                paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<void, tay::error_code> KernelSpace::unmap_high(HvaAddr address,
                                                                 size_t bytes) noexcept {
        auto state = page_table_.lock();
        return state->table.unmap(address.arith(), bytes, paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<void, tay::error_code> KernelSpace::protect_high(HvaAddr address, size_t bytes,
                                                                   PageFlags flags) noexcept {
        flags.user   = false;
        flags.global = true;
        auto state   = page_table_.lock();
        return state->table.protect(address.arith(), bytes, flags,
                                    paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<PageMapping, tay::error_code> KernelSpace::query(HvaAddr address) const noexcept {
        auto state = page_table_.lock();
        return state->table.query(address.arith(), paging::WalkDomain::KERNEL_OWNED);
    }

    PhyAddr KernelSpace::root() const noexcept {
        auto state = page_table_.lock();
        return state->table.root();
    }

    RootBinding KernelSpace::binding() const noexcept {
        return RootBinding{.client_root = guard_root_, .asid = 0, .role = RootRole::KERNEL};
    }

    void KernelSpace::activate() noexcept {
        hal::PageTableOps::activate_binding(binding());
    }

    tay::expected<RootSlotSnapshot, tay::error_code> KernelSpace::published_slot(
        HvaAddr address) const noexcept {
        const size_t index =
            hal::PageTableOps::index_at(address.arith(), hal::PageTableOps::TOP_LEVEL);
        auto state       = page_table_.lock();
        const auto entry = state->table.root_entry(index);
        if (!hal::PageTableOps::present(entry))
            return RootSlotSnapshot{.index = index, .entry = 0, .published = false};
        if (!hal::PageTableOps::leaf(entry)) {
            const PhyAddr child    = hal::PageTableOps::next_table(entry);
            const auto *descriptor = page_database().lookup(child);
            if (descriptor == nullptr || descriptor->state != PageState::CLAIMED ||
                descriptor->kind != PageKind::PAGE_TABLE ||
                descriptor->owner_id != KERNEL_PAGE_TABLE_OWNER)
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        return RootSlotSnapshot{.index = index, .entry = entry, .published = true};
    }

    tay::expected<void, tay::error_code> KernelSpace::copy_published_high_slots_to(
        PageTable &client) const noexcept {
        PageTable::EntryType entries[hal::PageTableOps::ENTRIES_PER_TABLE / 2]{};
        {
            auto state = page_table_.lock();
            for (size_t offset = 0; offset < hal::PageTableOps::ENTRIES_PER_TABLE / 2; ++offset)
                entries[offset] =
                    state->table.root_entry(offset + hal::PageTableOps::ENTRIES_PER_TABLE / 2);
        }
        for (size_t offset = 0; offset < hal::PageTableOps::ENTRIES_PER_TABLE / 2; ++offset) {
            const auto entry = entries[offset];
            if (!hal::PageTableOps::present(entry))
                continue;
            if (!client.install_root_entry_if_empty(
                    offset + hal::PageTableOps::ENTRIES_PER_TABLE / 2, entry))
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        return {};
    }

    bool kernel_space_ready() noexcept {
        return KernelSpace::ready_.load(std::memory_order_acquire);
    }

    KernelSpace *try_kernel_space() noexcept {
        return kernel_space_ready() ? &KernelSpace::instance_ : nullptr;
    }

    KernelSpace &kernel_space() noexcept {
        auto *space = try_kernel_space();
        if (space == nullptr)
            kernel::log::panic("KernelSpace 尚未初始化");
        return *space;
    }

    void init_kernel_space(const BootInfoHeader &bootinfo) noexcept {
        auto result = KernelSpace::initialize(bootinfo);
        if (!result)
            kernel::log::panic("无法初始化 KernelSpace: {}", result.error());
    }
}  // namespace memory
