/**
 * @file plic_error.h
 * @brief 定义 RISC-V PLIC 私有驱动错误；公共 IrqDomain 边界显式归约。
 */

#pragma once

#include <error.h>
#include <sustcore/addr.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace riscv64::device::interrupt {
    class PlicError final {
    public:
        using kernel_domain_error_tag = void;

        struct InvalidMmioRange {
            PhyArea area{};
        };
        struct ContextOutOfRange {
            u32_t context       = 0;
            u32_t context_count = 0;
        };
        struct SourceOutOfRange {
            u32_t source       = 0;
            u32_t source_count = 0;
        };
        struct InvalidPriority {
            u32_t priority = 0;
            u32_t maximum  = 0;
        };
        struct MissingMmio {};
        struct MissingContext {};
        struct InvalidClaim {};
        struct ControllerNotFound {};
        struct MmioFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct PlicAllocationFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct DomainRegistrationFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };

        PlicError()                                 = delete;
        PlicError(const PlicError &)                = default;
        PlicError &operator=(const PlicError &)     = default;
        PlicError(PlicError &&) noexcept            = default;
        PlicError &operator=(PlicError &&) noexcept = default;
        ~PlicError() noexcept                       = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::PlicError;
            return visit(tay::overloaded{
                [](const InvalidMmioRange &) noexcept { return Reason::INVALID_MMIO_RANGE; },
                [](const ContextOutOfRange &) noexcept { return Reason::CONTEXT_OUT_OF_RANGE; },
                [](const SourceOutOfRange &) noexcept { return Reason::SOURCE_OUT_OF_RANGE; },
                [](const InvalidPriority &) noexcept { return Reason::INVALID_PRIORITY; },
                [](const MissingMmio &) noexcept { return Reason::MISSING_MMIO; },
                [](const MissingContext &) noexcept { return Reason::MISSING_CONTEXT; },
                [](const InvalidClaim &) noexcept { return Reason::INVALID_CLAIM; },
                [](const ControllerNotFound &) noexcept { return Reason::CONTROLLER_NOT_FOUND; },
                [](const MmioFailed &) noexcept { return Reason::MMIO_FAILED; },
                [](const PlicAllocationFailed &) noexcept {
                    return Reason::PLIC_ALLOCATION_FAILED;
                },
                [](const DomainRegistrationFailed &) noexcept {
                    return Reason::DOMAIN_REGISTRATION_FAILED;
                },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidMmioRange &) noexcept { return "PLIC MMIO range is invalid"; },
                [](const ContextOutOfRange &) noexcept { return "PLIC context is out of range"; },
                [](const SourceOutOfRange &) noexcept { return "PLIC source is out of range"; },
                [](const InvalidPriority &) noexcept { return "PLIC priority is invalid"; },
                [](const MissingMmio &) noexcept { return "PLIC MMIO mapping is missing"; },
                [](const MissingContext &) noexcept { return "PLIC context is missing"; },
                [](const InvalidClaim &) noexcept { return "PLIC IRQ claim is invalid"; },
                [](const ControllerNotFound &) noexcept { return "PLIC controller was not found"; },
                [](const MmioFailed &) noexcept { return "PLIC MMIO operation failed"; },
                [](const PlicAllocationFailed &) noexcept { return "PLIC allocation failed"; },
                [](const DomainRegistrationFailed &) noexcept {
                    return "PLIC IRQ domain registration failed";
                },
            });
        }

    private:
        using Storage =
            tay::variant<InvalidMmioRange, ContextOutOfRange, SourceOutOfRange, InvalidPriority,
                         MissingMmio, MissingContext, InvalidClaim, ControllerNotFound, MmioFailed,
                         PlicAllocationFailed, DomainRegistrationFailed>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        PlicError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    [[nodiscard]] inline tay::error_code to_tay_error(const PlicError &error) noexcept {
        tay::optional<kernel::KernelError> cause;
        error.visit([&](const auto &value) noexcept {
            if constexpr (requires { value.cause; })
                cause = value.cause;
        });
        return kernel::to_tay_error(cause.value_or(error.code()));
    }

    static_assert(sizeof(PlicError) <= 40);
    static_assert(std::is_nothrow_move_constructible_v<PlicError>);
}  // namespace riscv64::device::interrupt
