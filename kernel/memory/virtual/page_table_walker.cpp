/**
 * @file page_table_walker.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 当前架构页表的单页遍历与变更原语
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/paging_traits.h>
#include <memory/virtual/page_table_walker.h>

#include <cstddef>

namespace memory::paging {
    namespace detail {
        using Ops       = hal::PageTableOps;
        using EntryType = Ops::EntryType;

        [[nodiscard]] constexpr size_t page_size_at_level(size_t level) noexcept {
            size_t result = PAGE_SIZE;
            for (size_t current = 0; current < level; ++current) result *= Ops::ENTRIES_PER_TABLE;
            return result;
        }

        [[nodiscard]] kernel::KernelError allocation_cause(tay::error_code error) noexcept {
            return kernel::from_tay_error(error).value_or(kernel::KernelError::TayError::INTERNAL);
        }

        void detach_subtree(PhyAddr physical, size_t level, RetirementSink &retirements,
                            LeafVisitor on_leaf) noexcept {
            const auto *entries = Ops::table(physical);
            for (size_t index = 0; index < Ops::ENTRIES_PER_TABLE; ++index) {
                const EntryType entry = Ops::load_entry(&entries[index]);
                if (!Ops::present(entry))
                    continue;
                if (Ops::leaf(entry)) {
                    on_leaf(Mapping{
                        .mapping   = PageMapping{.physical = Ops::leaf_physical(entry, 0, level),
                                                 .flags    = Ops::decode_flags(entry)},
                        .level     = level,
                        .page_size = page_size_at_level(level),
                    });
                } else if (level != 0) {
                    detach_subtree(Ops::next_table(entry), level - 1, retirements, on_leaf);
                }
            }
            retirements.defer_table(physical);
        }
    }  // namespace detail

    using detail::EntryType;
    using detail::Ops;
    using detail::page_size_at_level;

    tay::expected<Mapping, PagingError> Walker::query(addr_t address) const noexcept {
        if (!Ops::canonical(address))
            return tay::Err(
                PagingError::NonCanonicalAddress(PagingError::Operation::QUERY, address));

        PhyAddr current = root_;
        for (size_t level = Ops::TOP_LEVEL;; --level) {
            const auto *entries   = Ops::table(current);
            const EntryType entry = Ops::load_entry(&entries[Ops::index_at(address, level)]);
            if (!Ops::present(entry))
                return tay::Err(PagingError::MissingMapping(address));
            if (Ops::leaf(entry))
                return mapping_for(entry, address, level);
            if (level == 0)
                return tay::Err(PagingError::UnexpectedEntry(address, 0));
            current = Ops::next_table(entry);
        }
    }

    tay::expected<void, PagingError> Walker::try_map_base(addr_t address, PhyAddr physical,
                                                          const PageFlags &flags,
                                                          RetirementSink &retirements) noexcept {
        if (domain_ == WalkDomain::BORROWED_KERNEL_READ_ONLY)
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::MAP, address));
        if (!Ops::canonical(address))
            return tay::Err(PagingError::NonCanonicalAddress(PagingError::Operation::MAP, address));
        if ((address & (PAGE_SIZE - 1)) != 0)
            return tay::Err(
                PagingError::UnalignedRange(PagingError::Operation::MAP, address, PAGE_SIZE));
        if (!physical.aligned<PAGE_SIZE>())
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::MAP, physical));
        if (!valid_flags(flags))
            return tay::Err(PagingError::InvalidFlags(flags));

        struct CreatedTable {
            EntryType *parent = nullptr;
            PhyAddr physical{};
        };
        CreatedTable created[Ops::TOP_LEVEL]{};
        size_t created_count = 0;
        const auto rollback  = [&]() noexcept {
            while (created_count != 0) {
                auto &item = created[--created_count];
                Ops::store_leaf(item.parent, 0);
                retirements.defer_table(item.physical);
            }
        };

        PhyAddr current = root_;
        for (size_t level = Ops::TOP_LEVEL; level > 0; --level) {
            auto *entry              = &Ops::table(current)[Ops::index_at(address, level)];
            const EntryType existing = Ops::load_entry(entry);
            if (!Ops::present(existing)) {
                auto next = PageAllocator::allocate(owner_);
                if (!next) {
                    rollback();
                    return tay::Err(PagingError::PageTableAllocationFailed(
                        static_cast<u8_t>(level - 1), detail::allocation_cause(next.error())));
                }
                Ops::publish_table(entry, Ops::make_table(*next));
                created[created_count++] = CreatedTable{.parent = entry, .physical = *next};
                current                  = *next;
                continue;
            }
            if (Ops::leaf(existing)) {
                rollback();
                return tay::Err(PagingError::UnexpectedEntry(address, static_cast<u8_t>(level)));
            }
            current = Ops::next_table(existing);
        }

        auto *entry = &Ops::table(current)[Ops::index_at(address, 0)];
        if (Ops::present(Ops::load_entry(entry))) {
            rollback();
            return tay::Err(PagingError::MappingAlreadyPresent(address));
        }
        auto encoded = Ops::make_leaf(physical, flags);
        if (!encoded) {
            rollback();
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::MAP, physical));
        }
        Ops::store_leaf(entry, *encoded);
        return {};
    }

    tay::expected<Mapping, PagingError> Walker::protect_base(addr_t address,
                                                             const PageFlags &flags) noexcept {
        if (domain_ == WalkDomain::BORROWED_KERNEL_READ_ONLY)
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::PROTECT, address));
        if (!Ops::canonical(address))
            return tay::Err(
                PagingError::NonCanonicalAddress(PagingError::Operation::PROTECT, address));
        if ((address & (PAGE_SIZE - 1)) != 0)
            return tay::Err(
                PagingError::UnalignedRange(PagingError::Operation::PROTECT, address, PAGE_SIZE));
        if (!valid_flags(flags))
            return tay::Err(PagingError::InvalidFlags(flags));

        const EntryType leaf   = TAY_TRY(base_leaf(address));
        const Mapping previous = mapping_for(leaf, address, 0);
        auto encoded           = Ops::make_leaf(previous.mapping.physical, flags);
        if (!encoded)
            return tay::Err(PagingError::InvalidFlags(flags));
        Ops::store_leaf(leaf_entry(address), *encoded);
        return previous;
    }

    tay::expected<Mapping, PagingError> Walker::unmap_base(addr_t address,
                                                           RetirementSink &retirements) noexcept {
        if (domain_ == WalkDomain::BORROWED_KERNEL_READ_ONLY)
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::UNMAP, address));
        if (!Ops::canonical(address))
            return tay::Err(
                PagingError::NonCanonicalAddress(PagingError::Operation::UNMAP, address));
        if ((address & (PAGE_SIZE - 1)) != 0)
            return tay::Err(
                PagingError::UnalignedRange(PagingError::Operation::UNMAP, address, PAGE_SIZE));

        PhyAddr tables[Ops::TOP_LEVEL + 1]{root_};
        EntryType *parents[Ops::TOP_LEVEL]{};
        PhyAddr current = root_;
        for (size_t level = Ops::TOP_LEVEL; level > 0; --level) {
            auto *entry              = &Ops::table(current)[Ops::index_at(address, level)];
            const EntryType existing = Ops::load_entry(entry);
            if (!Ops::present(existing))
                return tay::Err(PagingError::MissingMapping(address));
            if (Ops::leaf(existing))
                return tay::Err(
                    PagingError::UnsupportedLeafLevel(address, static_cast<u8_t>(level)));
            const size_t depth = Ops::TOP_LEVEL - level;
            parents[depth]     = entry;
            current            = Ops::next_table(existing);
            tables[depth + 1]  = current;
        }

        auto *leaf               = &Ops::table(current)[Ops::index_at(address, 0)];
        const EntryType existing = Ops::load_entry(leaf);
        if (!Ops::present(existing))
            return tay::Err(PagingError::MissingMapping(address));
        if (!Ops::leaf(existing))
            return tay::Err(PagingError::UnexpectedEntry(address, 0));

        const Mapping previous = mapping_for(existing, address, 0);
        Ops::store_leaf(leaf, 0);
        for (size_t depth = Ops::TOP_LEVEL; depth > 0; --depth) {
            if (domain_ == WalkDomain::KERNEL_OWNED && depth == 1)
                break;
            if (!table_empty(tables[depth]))
                break;
            Ops::store_leaf(parents[depth - 1], 0);
            retirements.defer_table(tables[depth]);
        }
        return previous;
    }

    tay::expected<Mapping, PagingError> Walker::replace_base(addr_t address, PhyAddr physical,
                                                             const PageFlags &flags) noexcept {
        if (domain_ == WalkDomain::BORROWED_KERNEL_READ_ONLY)
            return tay::Err(
                PagingError::OutsideAddressDomain(PagingError::Operation::REPLACE, address));
        if (!Ops::canonical(address))
            return tay::Err(
                PagingError::NonCanonicalAddress(PagingError::Operation::REPLACE, address));
        if ((address & (PAGE_SIZE - 1)) != 0)
            return tay::Err(
                PagingError::UnalignedRange(PagingError::Operation::REPLACE, address, PAGE_SIZE));
        if (!physical.aligned<PAGE_SIZE>())
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::REPLACE, physical));
        if (!valid_flags(flags))
            return tay::Err(PagingError::InvalidFlags(flags));

        const EntryType leaf   = TAY_TRY(base_leaf(address));
        const Mapping previous = mapping_for(leaf, address, 0);
        auto encoded           = Ops::make_leaf(physical, flags);
        if (!encoded)
            return tay::Err(
                PagingError::InvalidPhysicalAddress(PagingError::Operation::REPLACE, physical));
        Ops::store_leaf(leaf_entry(address), *encoded);
        return previous;
    }

    bool Walker::valid_flags(const PageFlags &flags) noexcept {
        return !(flags.writable && flags.executable) && !(flags.writable && !flags.readable);
    }

    bool Walker::valid_base_address(addr_t address) noexcept {
        return (address & (PAGE_SIZE - 1)) == 0 && Ops::canonical(address);
    }

    Mapping Walker::mapping_for(EntryType entry, addr_t address, size_t level) noexcept {
        return Mapping{
            .mapping   = PageMapping{.physical = Ops::leaf_physical(entry, address, level),
                                     .flags    = Ops::decode_flags(entry)},
            .level     = level,
            .page_size = page_size_at_level(level),
        };
    }

    bool Walker::table_empty(PhyAddr physical) noexcept {
        const auto *entries = Ops::table(physical);
        for (size_t index = 0; index < Ops::ENTRIES_PER_TABLE; ++index)
            if (Ops::present(Ops::load_entry(&entries[index])))
                return false;
        return true;
    }

    tay::expected<Walker::EntryType, PagingError> Walker::base_leaf(addr_t address) const noexcept {
        PhyAddr current = root_;
        for (size_t level = Ops::TOP_LEVEL; level > 0; --level) {
            const EntryType entry =
                Ops::load_entry(&Ops::table(current)[Ops::index_at(address, level)]);
            if (!Ops::present(entry))
                return tay::Err(PagingError::MissingMapping(address));
            if (Ops::leaf(entry))
                return tay::Err(
                    PagingError::UnsupportedLeafLevel(address, static_cast<u8_t>(level)));
            current = Ops::next_table(entry);
        }
        const EntryType entry = Ops::load_entry(&Ops::table(current)[Ops::index_at(address, 0)]);
        if (!Ops::present(entry))
            return tay::Err(PagingError::MissingMapping(address));
        if (!Ops::leaf(entry))
            return tay::Err(PagingError::UnexpectedEntry(address, 0));
        return entry;
    }

    Walker::EntryType *Walker::leaf_entry(addr_t address) const noexcept {
        PhyAddr current = root_;
        for (size_t level = Ops::TOP_LEVEL; level > 0; --level) {
            const EntryType entry =
                Ops::load_entry(&Ops::table(current)[Ops::index_at(address, level)]);
            current = Ops::next_table(entry);
        }
        return &Ops::table(current)[Ops::index_at(address, 0)];
    }

    void detach_owned_tree(PhyAddr root, PageTableOwnerId owner, WalkDomain domain,
                           RetirementSink &retirements, LeafVisitor on_leaf) noexcept {
        static_cast<void>(owner);
        if (domain == WalkDomain::KERNEL_OWNED) {
            detail::detach_subtree(root, Ops::TOP_LEVEL, retirements, on_leaf);
            return;
        }

        auto *entries = Ops::table(root);
        for (size_t index = 0; index < Ops::ENTRIES_PER_TABLE / 2; ++index) {
            const EntryType entry = Ops::load_entry(&entries[index]);
            if (!Ops::present(entry))
                continue;
            if (Ops::leaf(entry)) {
                on_leaf(Mapping{
                    .mapping = PageMapping{.physical = Ops::leaf_physical(entry, 0, Ops::TOP_LEVEL),
                                           .flags    = Ops::decode_flags(entry)},
                    .level   = Ops::TOP_LEVEL,
                    .page_size = page_size_at_level(Ops::TOP_LEVEL),
                });
            } else {
                detail::detach_subtree(Ops::next_table(entry), Ops::TOP_LEVEL - 1, retirements,
                                       on_leaf);
            }
        }
        retirements.defer_table(root);
    }
}  // namespace memory::paging
