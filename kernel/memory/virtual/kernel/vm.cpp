/**
 * @file vm.cpp
 * @brief 全局 KernelVm 单例及其受锁保护的页表执行接口。
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/physical/page_db.h>
#include <memory/virtual/kernel/vm.h>

#include <cstddef>

namespace memory {
    constinit KernelVm KernelVm::instance_{};
    constinit std::atomic<bool> KernelVm::ready_{false};

    tay::expected<void, PagingError> KernelVm::initialize(const BootInfoHeader &) noexcept {
        if (ready_.load(std::memory_order_acquire))
            return tay::Err(PagingError::InvalidState(PagingError::Operation::ADOPT_ROOT));

        const PhyAddr root = TAY_TRY(PageTable::create_root(KERNEL_PAGE_TABLE_OWNER));
        auto guard         = PageTable::create_root(KERNEL_PAGE_TABLE_OWNER);
        if (!guard) {
            PageTable::destroy_root(root, KERNEL_PAGE_TABLE_OWNER);
            return TAY_ERR(guard);
        }

        {
            auto state   = instance_.page_table_.lock();
            auto adopted = state->table.adopt_root(root, PtKind::KERNEL, KERNEL_PAGE_TABLE_OWNER);
            if (!adopted) {
                PageTable::destroy_root(*guard, KERNEL_PAGE_TABLE_OWNER);
                PageTable::destroy_root(root, KERNEL_PAGE_TABLE_OWNER);
                return TAY_ERR(adopted);
            }
            instance_.guard_root_ = *guard;
        }
        ready_.store(true, std::memory_order_release);
        return {};
    }

    tay::expected<void, PagingError> KernelVm::map_high(HvaAddr address, PhyAddr physical,
                                                        size_t bytes, PageFlags flags) noexcept {
        flags.user   = false;
        flags.global = true;
        auto state   = page_table_.lock();
        return state->table.map(address.arith(), physical, bytes, flags,
                                paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<void, PagingError> KernelVm::unmap_high(HvaAddr address, size_t bytes) noexcept {
        auto state = page_table_.lock();
        return state->table.unmap(address.arith(), bytes, paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<void, PagingError> KernelVm::protect_high(HvaAddr address, size_t bytes,
                                                            PageFlags flags) noexcept {
        flags.user   = false;
        flags.global = true;
        auto state   = page_table_.lock();
        return state->table.protect(address.arith(), bytes, flags,
                                    paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<void, PagingError> KernelVm::map_trampoline(PhyAddr physical) noexcept {
        if (!physical.aligned<PAGE_SIZE>())
            return tay::Err(PagingError::UnalignedRange(PagingError::Operation::MAP,
                                                        physical.arith(), PAGE_SIZE));
        auto state = page_table_.lock();
        return state->table.map(
            physical.arith(), physical, PAGE_SIZE,
            PageFlags{.readable = true, .writable = false, .executable = true, .global = true},
            paging::WalkDomain::TRAMPOLINE_IDENTITY);
    }

    tay::expected<PageMapping, PagingError> KernelVm::query(HvaAddr address) const noexcept {
        auto state = page_table_.lock();
        return state->table.query(address.arith(), paging::WalkDomain::KERNEL_OWNED);
    }

    tay::expected<KvaAddr, PagingError> KernelVm::map_device(PhyAddr physical, size_t bytes,
                                                             PageFlags flags) noexcept {
        if (bytes == 0 || !physical.aligned<PAGE_SIZE>() || (bytes & (PAGE_SIZE - 1)) != 0)
            return tay::Err(
                PagingError::UnalignedRange(PagingError::Operation::MAP, physical.arith(), bytes));
        if (bytes > addr_t(-1) - physical.arith())
            return tay::Err(
                PagingError::RangeOverflow(PagingError::Operation::MAP, physical.arith(), bytes));
        if (physical.arith() > MAX_ADDR - KVA_START)
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::MAP, physical));
        if (!kernel_vm_ready())
            return tay::Err(PagingError::InvalidState(PagingError::Operation::MAP));
        const auto virtual_address = HvaAddr(PA2KVA(physical.arith()));
        flags.user                 = false;
        flags.global               = true;
        TAY_TRYV(map_high(virtual_address, physical, bytes, flags));
        return KvaAddr(virtual_address.arith());
    }

    tay::expected<void, PagingError> KernelVm::unmap_device(PhyAddr physical,
                                                            size_t bytes) noexcept {
        if (bytes == 0 || !physical.aligned<PAGE_SIZE>() || (bytes & (PAGE_SIZE - 1)) != 0)
            return tay::Err(PagingError::UnalignedRange(PagingError::Operation::UNMAP,
                                                        physical.arith(), bytes));
        if (bytes > addr_t(-1) - physical.arith())
            return tay::Err(
                PagingError::RangeOverflow(PagingError::Operation::UNMAP, physical.arith(), bytes));
        if (physical.arith() > MAX_ADDR - KVA_START)
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::UNMAP, physical));
        if (!kernel_vm_ready())
            return tay::Err(PagingError::InvalidState(PagingError::Operation::UNMAP));
        return unmap_high(HvaAddr(PA2KVA(physical.arith())), bytes);
    }

    PhyAddr KernelVm::root() const noexcept {
        auto state = page_table_.lock();
        return state->table.root();
    }

    RootBinding KernelVm::binding() const noexcept {
        return RootBinding{.private_root = guard_root_, .asid = 0, .role = RootRole::KERNEL};
    }

    void KernelVm::activate() noexcept {
        hal::PtOps::activate_binding(binding());
    }

    tay::expected<RootSlot, PagingError> KernelVm::published_slot(HvaAddr address) const noexcept {
        const size_t index = hal::PtOps::index_at(address.arith(), hal::PtOps::TOP_LEVEL);
        auto state         = page_table_.lock();
        const auto entry   = state->table.root_entry(index);
        if (!hal::PtOps::present(entry))
            return RootSlot{.index = index, .entry = 0, .published = false};
        if (!hal::PtOps::leaf(entry)) {
            const PhyAddr child    = hal::PtOps::next_table(entry);
            const auto *descriptor = page_db().lookup(child);
            if (descriptor == nullptr || descriptor->state != PageState::CLAIMED ||
                descriptor->kind != PageKind::PAGE_TABLE ||
                descriptor->owner_id != KERNEL_PAGE_TABLE_OWNER)
                return tay::Err(PagingError::UnexpectedEntry(
                    address.arith(), static_cast<u8_t>(hal::PtOps::TOP_LEVEL)));
        }
        return RootSlot{.index = index, .entry = entry, .published = true};
    }

    tay::expected<void, PagingError> KernelVm::copy_high_slots_to(
        PageTable &client) const noexcept {
        PageTable::EntryType entries[hal::PtOps::ENTRIES_PER_TABLE / 2]{};
        {
            auto state = page_table_.lock();
            for (size_t offset = 0; offset < hal::PtOps::ENTRIES_PER_TABLE / 2; ++offset)
                entries[offset] =
                    state->table.root_entry(offset + hal::PtOps::ENTRIES_PER_TABLE / 2);
        }
        for (size_t offset = 0; offset < hal::PtOps::ENTRIES_PER_TABLE / 2; ++offset) {
            const auto entry = entries[offset];
            if (!hal::PtOps::present(entry))
                continue;
            if (!client.try_install_root(offset + hal::PtOps::ENTRIES_PER_TABLE / 2,
                                                    entry))
                return tay::Err(PagingError::AlreadyMapped(0));
        }
        return {};
    }

    bool kernel_vm_ready() noexcept {
        return KernelVm::ready_.load(std::memory_order_acquire);
    }

    KernelVm *try_kernel_vm() noexcept {
        return kernel_vm_ready() ? &KernelVm::instance_ : nullptr;
    }

    KernelVm &kernel_vm() noexcept {
        auto *space = try_kernel_vm();
        if (space == nullptr)
            kernel::log::panic("KernelVm 尚未初始化");
        return *space;
    }

    void init_kernel_vm(const BootInfoHeader &bootinfo) noexcept {
        auto result = KernelVm::initialize(bootinfo);
        if (!result)
            kernel::log::panic("无法初始化 KernelVm: {}", result.error());
    }
}  // namespace memory
