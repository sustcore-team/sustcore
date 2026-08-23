/**
 * @file mem_seg.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 MemSeg 懒分配与访问错误。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <error.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memory {
    class MemSegError final {
    public:
        using kernel_domain_error_tag = void;

        struct ZeroSize {};
        struct SizeOverflow {
            size_t requested = 0;
        };
        struct OffsetOutOfRange {
            size_t offset = 0;
            size_t size   = 0;
        };
        struct PageNotAllocated {
            size_t page_index = 0;
        };
        struct InvalidSourceBuffer {};
        struct PhysAllocFailed {
            size_t page_index         = 0;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct PageInsertFailed {
            size_t page_index         = 0;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct OutOfMemory {};

        MemSegError()                                   = delete;
        MemSegError(const MemSegError &)                = default;
        MemSegError &operator=(const MemSegError &)     = default;
        MemSegError(MemSegError &&) noexcept            = default;
        MemSegError &operator=(MemSegError &&) noexcept = default;
        ~MemSegError() noexcept                         = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::MemSegError;
            return visit(tay::overloaded{
                [](const ZeroSize &) noexcept { return Reason::ZERO_SIZE; },
                [](const SizeOverflow &) noexcept { return Reason::SIZE_OVERFLOW; },
                [](const OffsetOutOfRange &) noexcept { return Reason::OFFSET_OUT_OF_RANGE; },
                [](const PageNotAllocated &) noexcept { return Reason::PAGE_NOT_ALLOCATED; },
                [](const InvalidSourceBuffer &) noexcept { return Reason::INVALID_SOURCE_BUFFER; },
                [](const PhysAllocFailed &) noexcept { return Reason::PHYS_ALLOC_FAILED; },
                [](const PageInsertFailed &) noexcept { return Reason::PAGE_INSERT_FAILED; },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const ZeroSize &) noexcept { return "memory segment size is zero"; },
                [](const SizeOverflow &) noexcept { return "memory segment size overflows"; },
                [](const OffsetOutOfRange &) noexcept {
                    return "memory segment offset is invalid";
                },
                [](const PageNotAllocated &) noexcept { return "memory segment page is sparse"; },
                [](const InvalidSourceBuffer &) noexcept {
                    return "memory source buffer is invalid";
                },
                [](const PhysAllocFailed &) noexcept {
                    return "memory segment physical allocation failed";
                },
                [](const PageInsertFailed &) noexcept {
                    return "memory segment page index insertion failed";
                },
                [](const OutOfMemory &) noexcept { return "memory segment allocation failed"; },
            });
        }

    private:
        using Storage =
            tay::variant<ZeroSize, SizeOverflow, OffsetOutOfRange, PageNotAllocated,
                         InvalidSourceBuffer, PhysAllocFailed, PageInsertFailed, OutOfMemory>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        MemSegError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(MemSegError) <= 32);
    static_assert(std::is_nothrow_move_constructible_v<MemSegError>);
}  // namespace memory
