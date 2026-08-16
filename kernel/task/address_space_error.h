/**
 * @file address_space_error.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义用户地址空间 VMA 与缺页处理错误。
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

namespace task {
    class AddressSpaceError final {
    public:
        using kernel_domain_error_tag = void;

        struct InvalidSegment {};
        struct InvalidArea {
            VirArea area{};
        };
        struct InvalidFlags {
            memory::PageFlags flags{};
        };
        struct SegmentOffsetOutOfRange {
            size_t offset       = 0;
            size_t segment_size = 0;
        };
        struct MappingExceedsSegment {
            size_t offset       = 0;
            size_t bytes        = 0;
            size_t segment_size = 0;
        };
        struct AccessDenied {
            memory::FaultAccess requested = memory::FaultAccess::NONE;
            memory::PageFlags allowed{};
        };
        struct VmaOverlap {
            VirArea requested{};
            VirArea existing{};
        };
        struct VmaNotOwned {};
        struct UnmappedAddress {
            VirAddr address{};
        };
        struct MappingChanged {
            VirAddr page{};
            u64_t expected_generation = 0;
        };
        struct BackingAllocationFailed {
            size_t segment_offset     = 0;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct PageTableFailed {
            VirAddr page{};
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct OutOfMemory {};

        AddressSpaceError()                                         = delete;
        AddressSpaceError(const AddressSpaceError &)                = default;
        AddressSpaceError &operator=(const AddressSpaceError &)     = default;
        AddressSpaceError(AddressSpaceError &&) noexcept            = default;
        AddressSpaceError &operator=(AddressSpaceError &&) noexcept = default;
        ~AddressSpaceError() noexcept                               = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::AddressSpaceError;
            return visit(tay::overloaded{
                [](const InvalidSegment &) noexcept { return Reason::INVALID_SEGMENT; },
                [](const InvalidArea &) noexcept { return Reason::INVALID_AREA; },
                [](const InvalidFlags &) noexcept { return Reason::INVALID_FLAGS; },
                [](const SegmentOffsetOutOfRange &) noexcept {
                    return Reason::SEGMENT_OFFSET_OUT_OF_RANGE;
                },
                [](const MappingExceedsSegment &) noexcept {
                    return Reason::MAPPING_EXCEEDS_SEGMENT;
                },
                [](const AccessDenied &) noexcept { return Reason::ACCESS_DENIED; },
                [](const VmaOverlap &) noexcept { return Reason::VMA_OVERLAP; },
                [](const VmaNotOwned &) noexcept { return Reason::VMA_NOT_OWNED; },
                [](const UnmappedAddress &) noexcept { return Reason::UNMAPPED_ADDRESS; },
                [](const MappingChanged &) noexcept { return Reason::MAPPING_CHANGED; },
                [](const BackingAllocationFailed &) noexcept {
                    return Reason::BACKING_ALLOCATION_FAILED;
                },
                [](const PageTableFailed &) noexcept { return Reason::PAGE_TABLE_FAILED; },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidSegment &) noexcept { return "invalid VMA backing segment"; },
                [](const InvalidArea &) noexcept { return "invalid VMA area"; },
                [](const InvalidFlags &) noexcept { return "invalid VMA flags"; },
                [](const SegmentOffsetOutOfRange &) noexcept {
                    return "VMA segment offset is out of range";
                },
                [](const MappingExceedsSegment &) noexcept {
                    return "VMA exceeds backing segment";
                },
                [](const AccessDenied &) noexcept { return "VMA access is denied"; },
                [](const VmaOverlap &) noexcept { return "VMA overlaps an existing mapping"; },
                [](const VmaNotOwned &) noexcept { return "VMA is not owned by address space"; },
                [](const UnmappedAddress &) noexcept { return "virtual address is unmapped"; },
                [](const MappingChanged &) noexcept { return "VMA changed during page fault"; },
                [](const BackingAllocationFailed &) noexcept {
                    return "VMA backing page allocation failed";
                },
                [](const PageTableFailed &) noexcept { return "address-space page-table failed"; },
                [](const OutOfMemory &) noexcept { return "address-space allocation failed"; },
            });
        }

    private:
        using Storage = tay::variant<InvalidSegment, InvalidArea, InvalidFlags,
                                     SegmentOffsetOutOfRange, MappingExceedsSegment, AccessDenied,
                                     VmaOverlap, VmaNotOwned, UnmappedAddress, MappingChanged,
                                     BackingAllocationFailed, PageTableFailed, OutOfMemory>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        AddressSpaceError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(AddressSpaceError) <= 48);
    static_assert(std::is_nothrow_move_constructible_v<AddressSpaceError>);
}  // namespace task
