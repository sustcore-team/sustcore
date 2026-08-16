/**
 * @file paging_error.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义页表遍历和映射事务的细粒度错误。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <error.h>
#include <memory/virtual/page_flags.h>
#include <sustcore/addr.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memory {
    /** @brief 页表操作可以恢复或向上层包装的结构化错误。 */
    class PagingError final {
    public:
        using kernel_domain_error_tag = void;

        enum class Operation : u8_t {
            MAP,
            UNMAP,
            PROTECT,
            QUERY,
            REPLACE,
            ADOPT_ROOT,
        };

        struct InvalidRoot {
            PhyAddr root{};
        };
        struct InvalidOwner {
            u64_t owner = 0;
        };
        struct InvalidState {
            Operation operation = Operation::QUERY;
        };
        enum class Identifier : u8_t {
            OWNER,
            ASID,
        };
        struct IdentifierExhausted {
            Identifier identifier = Identifier::OWNER;
        };
        struct NonCanonicalAddress {
            Operation operation = Operation::QUERY;
            addr_t address      = 0;
        };
        struct UnalignedRange {
            Operation operation = Operation::QUERY;
            addr_t address      = 0;
            size_t bytes        = 0;
        };
        struct RangeOverflow {
            Operation operation = Operation::QUERY;
            addr_t address      = 0;
            size_t bytes        = 0;
        };
        struct OutsideAddressDomain {
            Operation operation = Operation::QUERY;
            addr_t address      = 0;
        };
        struct InvalidPhysicalAddress {
            Operation operation = Operation::MAP;
            PhyAddr physical{};
        };
        struct InvalidFlags {
            PageFlags flags{};
        };
        struct MissingMapping {
            addr_t address = 0;
        };
        struct MappingAlreadyPresent {
            addr_t address = 0;
        };
        struct UnexpectedEntry {
            addr_t address = 0;
            u8_t level     = 0;
        };
        struct UnsupportedLeafLevel {
            addr_t address = 0;
            u8_t level     = 0;
        };
        struct PageTableAllocationFailed {
            u8_t level                = 0;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct OutOfMemory {};

        PagingError()                                   = delete;
        PagingError(const PagingError &)                = default;
        PagingError &operator=(const PagingError &)     = default;
        PagingError(PagingError &&) noexcept            = default;
        PagingError &operator=(PagingError &&) noexcept = default;
        ~PagingError() noexcept                         = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::PagingError;
            return visit(tay::overloaded{
                [](const InvalidRoot &) noexcept { return Reason::INVALID_ROOT; },
                [](const InvalidOwner &) noexcept { return Reason::INVALID_OWNER; },
                [](const InvalidState &) noexcept { return Reason::INVALID_STATE; },
                [](const IdentifierExhausted &) noexcept { return Reason::IDENTIFIER_EXHAUSTED; },
                [](const NonCanonicalAddress &) noexcept { return Reason::NON_CANONICAL_ADDRESS; },
                [](const UnalignedRange &) noexcept { return Reason::UNALIGNED_RANGE; },
                [](const RangeOverflow &) noexcept { return Reason::RANGE_OVERFLOW; },
                [](const OutsideAddressDomain &) noexcept {
                    return Reason::OUTSIDE_ADDRESS_DOMAIN;
                },
                [](const InvalidPhysicalAddress &) noexcept {
                    return Reason::INVALID_PHYSICAL_ADDRESS;
                },
                [](const InvalidFlags &) noexcept { return Reason::INVALID_FLAGS; },
                [](const MissingMapping &) noexcept { return Reason::MISSING_MAPPING; },
                [](const MappingAlreadyPresent &) noexcept {
                    return Reason::MAPPING_ALREADY_PRESENT;
                },
                [](const UnexpectedEntry &) noexcept { return Reason::UNEXPECTED_ENTRY; },
                [](const UnsupportedLeafLevel &) noexcept {
                    return Reason::UNSUPPORTED_LEAF_LEVEL;
                },
                [](const PageTableAllocationFailed &) noexcept {
                    return Reason::PAGE_TABLE_ALLOCATION_FAILED;
                },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidRoot &) noexcept { return "invalid page-table root"; },
                [](const InvalidOwner &) noexcept { return "invalid page-table owner"; },
                [](const InvalidState &) noexcept { return "invalid paging state"; },
                [](const IdentifierExhausted &) noexcept { return "paging identifier exhausted"; },
                [](const NonCanonicalAddress &) noexcept {
                    return "non-canonical virtual address";
                },
                [](const UnalignedRange &) noexcept { return "unaligned page-table range"; },
                [](const RangeOverflow &) noexcept { return "page-table range overflow"; },
                [](const OutsideAddressDomain &) noexcept {
                    return "address is outside the page-table domain";
                },
                [](const InvalidPhysicalAddress &) noexcept { return "invalid physical address"; },
                [](const InvalidFlags &) noexcept { return "invalid page flags"; },
                [](const MissingMapping &) noexcept { return "page mapping is missing"; },
                [](const MappingAlreadyPresent &) noexcept {
                    return "page mapping already exists";
                },
                [](const UnexpectedEntry &) noexcept { return "unexpected page-table entry"; },
                [](const UnsupportedLeafLevel &) noexcept { return "unsupported page leaf level"; },
                [](const PageTableAllocationFailed &) noexcept {
                    return "page-table page allocation failed";
                },
                [](const OutOfMemory &) noexcept { return "paging object allocation failed"; },
            });
        }

    private:
        using Storage = tay::variant<InvalidRoot, InvalidOwner, InvalidState, IdentifierExhausted,
                                     NonCanonicalAddress, UnalignedRange, RangeOverflow,
                                     OutsideAddressDomain, InvalidPhysicalAddress, InvalidFlags,
                                     MissingMapping, MappingAlreadyPresent, UnexpectedEntry,
                                     UnsupportedLeafLevel, PageTableAllocationFailed, OutOfMemory>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        PagingError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(PagingError) <= 32);
    static_assert(std::is_nothrow_move_constructible_v<PagingError>);
}  // namespace memory
