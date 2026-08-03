/**
 * @file client_space.cpp
 * @brief ClientSpace 三阶段工厂与 RISC-V 借用根项修复。
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/virtual/client/client_space.h>

#include <atomic>
#include <utility>

namespace memory {
    namespace {
        constinit std::atomic<PageTableOwnerId> next_owner{0x100};
        constinit std::atomic<u16_t> next_asid{1};
        ClientSpace *current_client = nullptr;
    }  // namespace

    tay::expected<ClientSpace, tay::error_code> ClientSpace::create() noexcept {
        const PageTableOwnerId owner = next_owner.fetch_add(1, std::memory_order_relaxed);
        const u16_t asid             = next_asid.fetch_add(1, std::memory_order_relaxed);
        if (owner == 0 || asid == 0 || asid > 0x03ff)
            return tay::expected<ClientSpace, tay::error_code>(tay::unexpect,
                                                               tay::error_code::OUT_OF_RANGE);
        auto root = PageTable::create_root(owner);
        if (!root)
            return tay::expected<ClientSpace, tay::error_code>(tay::unexpect, root.error());

        Resources resources{.root = *root, .owner = owner, .asid = asid};
        return tay::expected<ClientSpace, tay::error_code>(
            tay::try_in_place, [](ClientSpace &candidate) noexcept { return candidate.init(); },
            resources);
    }

    tay::expected<void, tay::error_code> ClientSpace::init() noexcept {
        if constexpr (hal::PageTableOps::SHARES_HIGH_ROOT) {
            auto table = page_table_.lock();
            return kernel_space().copy_published_high_slots_to(*table);
        }
        return {};
    }

    tay::expected<void, tay::error_code> ClientSpace::map(VirAddr address, PhyAddr physical,
                                                          size_t bytes, PageFlags flags) noexcept {
        flags.user   = true;
        flags.global = false;
        auto table   = page_table_.lock();
        return table->map(address.arith(), physical, bytes, flags,
                          paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<void, tay::error_code> ClientSpace::unmap(VirAddr address,
                                                            size_t bytes) noexcept {
        auto table = page_table_.lock();
        return table->unmap(address.arith(), bytes, paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<void, tay::error_code> ClientSpace::protect(VirAddr address, size_t bytes,
                                                              PageFlags flags) noexcept {
        flags.user   = true;
        flags.global = false;
        auto table   = page_table_.lock();
        return table->protect(address.arith(), bytes, flags, paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<PageMapping, tay::error_code> ClientSpace::query(VirAddr address) const noexcept {
        auto table = page_table_.lock();
        return table->query(address.arith(), paging::WalkDomain::USER_PRIVATE);
    }

    tay::expected<BorrowedSlotRepair, tay::error_code>
    ClientSpace::repair_missing_borrowed_kernel_slot(HvaAddr address) noexcept {
        if constexpr (!hal::PageTableOps::SHARES_HIGH_ROOT)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto snapshot = kernel_space().published_slot(address);
        if (!snapshot)
            return tay::Err(snapshot.error());
        if (!snapshot->published)
            return BorrowedSlotRepair::GLOBAL_SLOT_ABSENT;

        auto table = page_table_.lock();
        if (hal::PageTableOps::present(table->root_entry(snapshot->index)))
            return BorrowedSlotRepair::LOCAL_SLOT_PRESENT;
        if (!table->install_root_entry_if_empty(snapshot->index, snapshot->entry))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        hal::PageTableOps::flush_tlb();
        return BorrowedSlotRepair::REPAIRED;
    }

    RootBinding ClientSpace::binding() const noexcept {
        auto table = page_table_.lock();
        return RootBinding{.client_root = table->root(), .asid = asid_, .role = RootRole::CLIENT};
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
