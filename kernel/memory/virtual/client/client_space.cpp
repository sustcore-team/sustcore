/**
 * @file client_space.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ClientSpace 三阶段工厂与 RISC-V 借用根项修复。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/virtual/client/client_space.h>
#include <tay/counter.h>

#include <limits>
#include <new>
#include <utility>

namespace memory {
    namespace {
        constinit tay::counter<PageTableOwnerId> page_table_owners{0x100};
        constinit tay::counter<u32_t> address_space_ids{1};
        ClientSpace *current_client = nullptr;
    }  // namespace

    tay::expected<ClientSpace *, PagingError> ClientSpace::create() noexcept {
        PageTableOwnerId owner = 0;
        u32_t asid_value       = 0;
        if (!page_table_owners.try_next(std::numeric_limits<PageTableOwnerId>::max() - 1, owner))
            return tay::Err(PagingError::IdentifierExhausted(PagingError::Identifier::OWNER));
        if (!address_space_ids.try_next(0x03ff, asid_value))
            return tay::Err(PagingError::IdentifierExhausted(PagingError::Identifier::ASID));
        const PhyAddr root = TAY_TRY(PageTable::create_root(owner));
        auto *space        = new (std::nothrow) ClientSpace(
            Resources{.root = root, .owner = owner, .asid = static_cast<u16_t>(asid_value)});
        if (space == nullptr) {
            PageTable::destroy_root(root, owner);
            return tay::Err(PagingError::OutOfMemory());
        }
        if (auto initialized = space->init(); !initialized) {
            auto error = initialized.error();
            delete space;
            return tay::Err(error);
        }
        return space;
    }

    tay::expected<void, PagingError> ClientSpace::init() noexcept {
        if constexpr (hal::PageTableOps::SHARES_HIGH_ROOT) {
            auto table = page_table_.lock();
            return kernel_space().copy_published_high_slots_to(*table);
        }
        return {};
    }

    tay::expected<void, PagingError> ClientSpace::map(VirAddr address, PhyAddr physical,
                                                      size_t bytes, PageFlags flags) noexcept {
        flags.user   = true;
        flags.global = false;
        auto table   = page_table_.lock();
        return table->map(address.arith(), physical, bytes, flags,
                          paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<void, PagingError> ClientSpace::unmap(VirAddr address, size_t bytes) noexcept {
        auto table = page_table_.lock();
        return table->unmap(address.arith(), bytes, paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<void, PagingError> ClientSpace::protect(VirAddr address, size_t bytes,
                                                          PageFlags flags) noexcept {
        flags.user   = true;
        flags.global = false;
        auto table   = page_table_.lock();
        return table->protect(address.arith(), bytes, flags, paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<PageMapping, PagingError> ClientSpace::query(VirAddr address) const noexcept {
        auto table = page_table_.lock();
        return table->query(address.arith(), paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<BorrowedSlotRepair, PagingError> ClientSpace::repair_missing_borrowed_kernel_slot(
        HvaAddr address) noexcept {
        if constexpr (!hal::PageTableOps::SHARES_HIGH_ROOT)
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::QUERY, address.arith()));

        const auto snapshot = TAY_TRY(kernel_space().published_slot(address));
        if (!snapshot.published)
            return BorrowedSlotRepair::GLOBAL_SLOT_ABSENT;

        auto table = page_table_.lock();
        if (hal::PageTableOps::present(table->root_entry(snapshot.index)))
            return BorrowedSlotRepair::LOCAL_SLOT_PRESENT;
        if (!table->install_root_entry_if_empty(snapshot.index, snapshot.entry))
            return tay::Err(PagingError::MappingAlreadyPresent(address.arith()));
        hal::PageTableOps::flush_tlb();
        return BorrowedSlotRepair::REPAIRED;
    }

    RootBinding ClientSpace::binding() const noexcept {
        auto table = page_table_.lock();
        return RootBinding{.private_root = table->root(), .asid = asid_, .role = RootRole::CLIENT};
    }

    void ClientSpace::activate() noexcept {
        current_client = this;
        hal::PageTableOps::activate_binding(binding());
    }

    ClientSpace *active_client_space() noexcept {
        return current_client;
    }

    void activate_kernel_space() noexcept {
        current_client = nullptr;
        kernel_space().activate();
    }
}  // namespace memory
