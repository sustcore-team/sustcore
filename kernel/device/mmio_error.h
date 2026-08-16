/**
 * @file mmio_error.h
 * @brief 定义 MMIO capability 对象与内核映射错误。
 */

#pragma once

#include <error.h>
#include <memory/virtual/paging_error.h>
#include <sustcore/addr.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace device {
    class MmioError final {
    public:
        using kernel_domain_error_tag = void;

        struct InvalidPhysicalArea {
            PhyArea area{};
        };
        struct SizeOverflow {
            PhyAddr physical{};
            size_t bytes = 0;
        };
        struct MappingConflict {
            PhyArea area{};
        };
        struct KernelSpaceUnavailable {};
        struct NotMapped {};
        struct PagingFailed {
            memory::PagingError error;
        };
        struct OutOfMemory {};

        MmioError()                                 = delete;
        MmioError(const MmioError &)                = default;
        MmioError &operator=(const MmioError &)     = default;
        MmioError(MmioError &&) noexcept            = default;
        MmioError &operator=(MmioError &&) noexcept = default;
        ~MmioError() noexcept                       = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::MmioError;
            return visit(tay::overloaded{
                [](const InvalidPhysicalArea &) noexcept { return Reason::INVALID_PHYSICAL_AREA; },
                [](const SizeOverflow &) noexcept { return Reason::SIZE_OVERFLOW; },
                [](const MappingConflict &) noexcept { return Reason::MAPPING_CONFLICT; },
                [](const KernelSpaceUnavailable &) noexcept {
                    return Reason::KERNEL_SPACE_UNAVAILABLE;
                },
                [](const NotMapped &) noexcept { return Reason::NOT_MAPPED; },
                [](const PagingFailed &) noexcept { return Reason::PAGING_FAILED; },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidPhysicalArea &) noexcept { return "invalid MMIO physical area"; },
                [](const SizeOverflow &) noexcept { return "MMIO physical range overflows"; },
                [](const MappingConflict &) noexcept { return "MMIO object is already mapped"; },
                [](const KernelSpaceUnavailable &) noexcept {
                    return "kernel address space is unavailable";
                },
                [](const NotMapped &) noexcept { return "MMIO object is not mapped"; },
                [](const PagingFailed &) noexcept { return "MMIO paging operation failed"; },
                [](const OutOfMemory &) noexcept { return "MMIO object allocation failed"; },
            });
        }

    private:
        using Storage = tay::variant<InvalidPhysicalArea, SizeOverflow, MappingConflict,
                                     KernelSpaceUnavailable, NotMapped, PagingFailed, OutOfMemory>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        MmioError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(MmioError) <= 40);
    static_assert(std::is_nothrow_move_constructible_v<MmioError>);
}  // namespace device
