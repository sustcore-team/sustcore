/**
 * @file vm.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief UserVm 三阶段工厂与 RISC-V 借用根项修复。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/paging_traits.h>
#include <cpu/local.h>
#include <log.h>
#include <memory/virtual/user/vm.h>
#include <tay/counter.h>
#include <tay/guard.h>
#include <tay/unique_ptr.h>

#include <limits>
#include <new>
#include <utility>

namespace memory {
    namespace {
        constinit tay::counter<PtOwnerId> page_table_owners{0x100};
        constinit tay::counter<u32_t> address_space_ids{1};
    }  // namespace

    tay::expected<UserVm *, PagingError> UserVm::create() noexcept {
        PtOwnerId owner  = 0;
        u32_t asid_value = 0;
        if (!page_table_owners.try_next(std::numeric_limits<PtOwnerId>::max() - 1, owner))
            return tay::Err(PagingError::IdentifierExhausted(PagingError::Identifier::OWNER));
        if (!address_space_ids.try_next(0x03ff, asid_value))
            return tay::Err(PagingError::IdentifierExhausted(PagingError::Identifier::ASID));
        const PhyAddr root = TAY_TRY(PageTable::create_root(owner));
        tay::guard root_guard([root, owner]() noexcept { PageTable::destroy_root(root, owner); });
        tay::unique_ptr<UserVm> space(new (std::nothrow) UserVm(
            InitArgs{.root = root, .owner = owner, .asid = static_cast<u16_t>(asid_value)}));
        if (!space)
            return tay::Err(PagingError::OutOfMemory());
        root_guard.release();
        if (auto initialized = space->init(); !initialized) {
            auto error = initialized.error();
            return tay::Err(error);
        }
        return space.release();
    }

    tay::expected<void, PagingError> UserVm::init() noexcept {
        if constexpr (hal::PtOps::SHARES_HIGH_ROOT) {
            auto table = page_table_.lock();
            return kernel_vm().copy_high_slots_to(*table);
        }
        return {};
    }

    tay::expected<void, PagingError> UserVm::map(VirAddr address, PhyAddr physical, size_t bytes,
                                                 PageFlags flags) noexcept {
        flags.user   = true;
        flags.global = false;
        auto table   = page_table_.lock();
        return table->map(address.arith(), physical, bytes, flags,
                          paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<void, PagingError> UserVm::unmap(VirAddr address, size_t bytes) noexcept {
        auto table = page_table_.lock();
        return table->unmap(address.arith(), bytes, paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<void, PagingError> UserVm::protect(VirAddr address, size_t bytes,
                                                     PageFlags flags) noexcept {
        flags.user   = true;
        flags.global = false;
        auto table   = page_table_.lock();
        return table->protect(address.arith(), bytes, flags, paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<PageMapping, PagingError> UserVm::query(VirAddr address) const noexcept {
        auto table = page_table_.lock();
        return table->query(address.arith(), paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<BorrowedSlotFix, PagingError> UserVm::fix_borrowed_slot(
        HvaAddr address) noexcept {
        if constexpr (!hal::PtOps::SHARES_HIGH_ROOT)
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::QUERY, address.arith()));

        const auto snapshot = TAY_TRY(kernel_vm().published_slot(address));
        if (!snapshot.published)
            return BorrowedSlotFix::GLOBAL_SLOT_ABSENT;

        auto table = page_table_.lock();
        if (hal::PtOps::present(table->root_entry(snapshot.index)))
            return BorrowedSlotFix::LOCAL_SLOT_PRESENT;
        if (!table->try_install_root(snapshot.index, snapshot.entry))
            return tay::Err(PagingError::AlreadyMapped(address.arith()));
        table->shootdown(address.arith(), 1);
        return BorrowedSlotFix::REPAIRED;
    }

    RootBinding UserVm::binding() const noexcept {
        auto table = page_table_.lock();
        return RootBinding{.private_root = table->root(), .asid = asid_, .role = RootRole::CLIENT};
    }

    void UserVm::activate() noexcept {
        cpu::local().active_user_vm = this;
        hal::PtOps::activate_binding(binding());
    }

    UserVm *active_user_vm() noexcept {
        return cpu::local().active_user_vm;
    }

    void activate_kernel_vm() noexcept {
        cpu::local().active_user_vm = nullptr;
        kernel_vm().activate();
    }
}  // namespace memory
