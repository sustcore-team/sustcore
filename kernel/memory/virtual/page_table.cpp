/**
 * @file page_table.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页表范围操作、同步与对象生命周期
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/paging_traits.h>
#include <log.h>
#include <memory/physical/page_database.h>
#include <memory/virtual/page_table.h>
#include <memory/virtual/page_table_walker.h>
#include <sustcore/addrspace.h>

#include <cstddef>

namespace memory {
    namespace {
        [[nodiscard]] bool domain_accepts(addr_t address, size_t bytes,
                                          paging::WalkDomain domain) noexcept {
            const addr_t last = address + bytes - 1;
            if (domain == paging::WalkDomain::USER_PRIVATE)
                return address < KPA_START && last < KPA_START;
            return address >= KPA_START && last >= KPA_START;
        }
    }  // namespace

    tay::expected<PhyAddr, PagingError> PageTable::create_root(PageTableOwnerId owner) noexcept {
        if (owner == 0)
            return tay::Err(PagingError::InvalidOwner(owner));
        return paging::PageAllocator::allocate(owner).transform_error(
            [](tay::error_code error) noexcept -> PagingError {
                return PagingError::PageTableAllocationFailed(
                    static_cast<u8_t>(hal::PageTableOps::TOP_LEVEL),
                    kernel::from_tay_error(error).value_or(
                        kernel::KernelError::TayError::INTERNAL));
            });
    }

    void PageTable::destroy_root(PhyAddr root, PageTableOwnerId owner) noexcept {
        paging::PageAllocator::retire(root, owner);
    }

    PageTable::PageTable(PhyAddr root, PageTableKind kind, PageTableOwnerId owner) noexcept {
        if (!adopt_root(root, kind, owner))
            kernel::log::panic("无效的显式 PageTable 根节点");
    }

    tay::expected<void, PagingError> PageTable::adopt_root(PhyAddr root, PageTableKind kind,
                                                           PageTableOwnerId owner) noexcept {
        const auto *descriptor = page_database().lookup(root);
        if (root_.nonnull() || !root.nonnull() || !root.aligned<PAGE_SIZE>() || owner == 0 ||
            descriptor == nullptr || descriptor->state != PageState::CLAIMED ||
            descriptor->kind != PageKind::PAGE_TABLE || descriptor->owner_id != owner)
            return tay::Err(PagingError::InvalidRoot(root));
        root_  = root;
        kind_  = kind;
        owner_ = owner;
        return {};
    }

    PageTable::~PageTable() noexcept {
        if (!root_.nonnull())
            return;
        paging::RetirementSink retirements(owner_);
        paging::detach_owned_tree(
            root_, owner_,
            kind_ == PageTableKind::KERNEL ? paging::WalkDomain::KERNEL_OWNED
                                           : paging::WalkDomain::USER_PRIVATE,
            retirements, [](const paging::Mapping &mapping) noexcept {
                if (auto *descriptor =
                        page_database().lookup(mapping.mapping.physical.align_down(PAGE_SIZE));
                    descriptor != nullptr)
                    descriptor->map_count.fetch_sub(1, std::memory_order_relaxed);
            });
        hal::PageTableOps::flush_tlb();
        retirements.retire_all();
    }

    tay::expected<size_t, PagingError> PageTable::checked_range(
        addr_t vaddr, size_t bytes, PagingError::Operation operation) noexcept {
        if (bytes == 0 || (vaddr & (PAGE_SIZE - 1)) != 0 || (bytes & (PAGE_SIZE - 1)) != 0)
            return tay::Err(PagingError::UnalignedRange(operation, vaddr, bytes));
        if (bytes > addr_t(-1) - vaddr)
            return tay::Err(PagingError::RangeOverflow(operation, vaddr, bytes));
        const addr_t last = vaddr + bytes - 1;
        if (!hal::PageTableOps::canonical(vaddr))
            return tay::Err(PagingError::NonCanonicalAddress(operation, vaddr));
        if (!hal::PageTableOps::canonical(last))
            return tay::Err(PagingError::NonCanonicalAddress(operation, last));
        return bytes / PAGE_SIZE;
    }

    bool PageTable::valid_flags(const PageFlags &flags) noexcept {
        return !(flags.writable && flags.executable) && !(flags.writable && !flags.readable);
    }

    tay::expected<void, PagingError> PageTable::map(addr_t vaddr, PhyAddr physical, size_t bytes,
                                                    PageFlags flags,
                                                    paging::WalkDomain domain) noexcept {
        const size_t pages = TAY_TRY(checked_range(vaddr, bytes, PagingError::Operation::MAP));
        if (!physical.aligned<PAGE_SIZE>())
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::MAP, physical));
        if (bytes > addr_t(-1) - physical.arith())
            return tay::Err(
                PagingError::RangeOverflow(PagingError::Operation::MAP, physical.arith(), bytes));
        if (!valid_flags(flags))
            return tay::Err(PagingError::InvalidFlags(flags));
        if (!domain_accepts(vaddr, bytes, domain))
            return tay::Err(PagingError::OutsideAddressDomain(PagingError::Operation::MAP, vaddr));
        paging::RetirementSink retirements(owner_);
        paging::Walker walker(root_, owner_, domain);
        size_t mapped = 0;
        for (; mapped < pages; ++mapped) {
            auto result = walker.try_map_base(vaddr + mapped * PAGE_SIZE,
                                              physical + mapped * PAGE_SIZE, flags, retirements);
            if (!result) {
                while (mapped != 0) {
                    --mapped;
                    auto unmapped = walker.unmap_base(vaddr + mapped * PAGE_SIZE, retirements);
                    if (!unmapped)
                        kernel::log::panic("PageTable 映射回滚丢失了映射");
                    if (auto *descriptor = page_database().lookup(
                            unmapped->mapping.physical.align_down(PAGE_SIZE));
                        descriptor != nullptr)
                        descriptor->map_count.fetch_sub(1, std::memory_order_relaxed);
                }
                hal::PageTableOps::flush_tlb();
                retirements.retire_all();
                return TAY_ERR(result);
            }
            if (auto *descriptor = page_database().lookup(physical + mapped * PAGE_SIZE);
                descriptor != nullptr)
                descriptor->map_count.fetch_add(1, std::memory_order_relaxed);
        }
        hal::PageTableOps::flush_tlb();
        retirements.retire_all();
        return {};
    }

    tay::expected<void, PagingError> PageTable::unmap(addr_t vaddr, size_t bytes,
                                                      paging::WalkDomain domain) noexcept {
        const size_t pages = TAY_TRY(checked_range(vaddr, bytes, PagingError::Operation::UNMAP));
        if (!domain_accepts(vaddr, bytes, domain))
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::UNMAP, vaddr));
        paging::RetirementSink retirements(owner_);
        paging::Walker walker(root_, owner_, domain);
        for (size_t page = 0; page < pages; ++page) {
            const auto mapping = TAY_TRY(walker.query(vaddr + page * PAGE_SIZE));
            if (mapping.level != 0)
                return tay::Err(PagingError::UnsupportedLeafLevel(
                    vaddr + page * PAGE_SIZE, static_cast<u8_t>(mapping.level)));
        }
        for (size_t page = 0; page < pages; ++page) {
            auto mapping = walker.unmap_base(vaddr + page * PAGE_SIZE, retirements);
            if (!mapping)
                kernel::log::panic("预检成功后 PageTable 取消映射结果发生变化");
            if (mapping) {
                auto *descriptor =
                    page_database().lookup(mapping->mapping.physical.align_down(PAGE_SIZE));
                if (descriptor != nullptr)
                    descriptor->map_count.fetch_sub(1, std::memory_order_relaxed);
            }
        }
        hal::PageTableOps::flush_tlb();
        retirements.retire_all();
        return {};
    }

    tay::expected<void, PagingError> PageTable::protect(addr_t vaddr, size_t bytes, PageFlags flags,
                                                        paging::WalkDomain domain) noexcept {
        const size_t pages = TAY_TRY(checked_range(vaddr, bytes, PagingError::Operation::PROTECT));
        if (!valid_flags(flags))
            return tay::Err(PagingError::InvalidFlags(flags));
        if (!domain_accepts(vaddr, bytes, domain))
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::PROTECT, vaddr));
        paging::Walker walker(root_, owner_, domain);
        for (size_t page = 0; page < pages; ++page) {
            const auto mapping = TAY_TRY(walker.query(vaddr + page * PAGE_SIZE));
            if (mapping.level != 0)
                return tay::Err(PagingError::UnsupportedLeafLevel(
                    vaddr + page * PAGE_SIZE, static_cast<u8_t>(mapping.level)));
        }
        for (size_t page = 0; page < pages; ++page) {
            auto result = walker.protect_base(vaddr + page * PAGE_SIZE, flags);
            if (!result)
                kernel::log::panic("预检成功后 PageTable 保护结果发生变化");
        }
        hal::PageTableOps::flush_tlb();
        return {};
    }

    tay::expected<PageMapping, PagingError> PageTable::query(
        addr_t vaddr, paging::WalkDomain domain) const noexcept {
        if (!hal::PageTableOps::canonical(vaddr))
            return tay::Err(PagingError::NonCanonicalAddress(PagingError::Operation::QUERY, vaddr));
        if (!domain_accepts(vaddr, 1, domain))
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::QUERY, vaddr));
        return TAY_TRY(paging::Walker(root_, owner_, domain).query(vaddr)).mapping;
    }

    PageTable::EntryType PageTable::root_entry(size_t index) const noexcept {
        if (index >= hal::PageTableOps::ENTRIES_PER_TABLE)
            return 0;
        return hal::PageTableOps::load_entry(&hal::PageTableOps::table(root_)[index]);
    }

    bool PageTable::install_root_entry_if_empty(size_t index, EntryType value) noexcept {
        if (index >= hal::PageTableOps::ENTRIES_PER_TABLE || !hal::PageTableOps::present(value))
            return false;
        auto *entry = &hal::PageTableOps::table(root_)[index];
        if (hal::PageTableOps::present(hal::PageTableOps::load_entry(entry)))
            return false;
        hal::PageTableOps::publish_table(entry, value);
        return true;
    }
}  // namespace memory
