/**
 * @file kernel_layout_error.h
 * @brief 定义 KernelMM 布局所有权与映射事务错误。
 */

#pragma once

#include <error.h>
#include <memory/virtual/kernel/kernel_layout.h>
#include <memory/virtual/paging_error.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace memory {
    class KernelLayoutError final {
    public:
        using kernel_domain_error_tag = void;

        enum class LayoutKind : u8_t {
            KERNEL,
            HHDM,
            RESERVED,
        };

        enum class Dependency : u8_t {
            HEAP,
            KERNEL_SPACE,
        };

        struct InitializationAlreadyAttempted {};
        struct DependencyNotReady {
            Dependency dependency = Dependency::HEAP;
        };
        struct InvalidKernelLayout {
            KernelLayoutSpec layout{};
        };
        struct InvalidHhdmLayout {
            HHDMLayout layout{};
        };
        struct InvalidReservedLayout {
            ReservedLayout layout{};
        };
        struct LayoutConflict {
            KvaArea requested{};
            KvaArea existing{};
        };
        struct HhdmConflict {
            PhyArea requested{};
            PhyArea existing{};
        };
        struct KernelLayoutNotFound {
            KernelLayoutId id = 0;
        };
        struct HhdmLayoutNotFound {
            HHDMLayoutId id = 0;
        };
        struct ReservedLayoutNotFound {
            ReservedLayoutId id = 0;
        };
        struct HhdmCoverageMissing {
            PhyAddr physical_base{};
            size_t bytes = 0;
        };
        struct ReservationOwnedByKernel {
            ReservedLayoutId reservation = 0;
            KernelLayoutId owner         = 0;
        };
        struct ReservationsPresent {
            HHDMLayoutId id = 0;
        };
        struct OwnershipMismatch {
            KernelLayoutId id = 0;
        };
        struct NodeAllocationFailed {
            LayoutKind kind = LayoutKind::KERNEL;
            bool bootstrap  = false;
        };
        struct PagingFailed {
            PagingError error;
        };

        KernelLayoutError()                                         = delete;
        KernelLayoutError(const KernelLayoutError &)                = default;
        KernelLayoutError &operator=(const KernelLayoutError &)     = default;
        KernelLayoutError(KernelLayoutError &&) noexcept            = default;
        KernelLayoutError &operator=(KernelLayoutError &&) noexcept = default;
        ~KernelLayoutError() noexcept                               = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::KernelLayoutError;
            return visit(tay::overloaded{
                [](const InitializationAlreadyAttempted &) noexcept {
                    return Reason::INITIALIZATION_ALREADY_ATTEMPTED;
                },
                [](const DependencyNotReady &) noexcept { return Reason::DEPENDENCY_NOT_READY; },
                [](const InvalidKernelLayout &) noexcept { return Reason::INVALID_KERNEL_LAYOUT; },
                [](const InvalidHhdmLayout &) noexcept { return Reason::INVALID_HHDM_LAYOUT; },
                [](const InvalidReservedLayout &) noexcept {
                    return Reason::INVALID_RESERVED_LAYOUT;
                },
                [](const LayoutConflict &) noexcept { return Reason::LAYOUT_CONFLICT; },
                [](const HhdmConflict &) noexcept { return Reason::HHDM_CONFLICT; },
                [](const KernelLayoutNotFound &) noexcept {
                    return Reason::KERNEL_LAYOUT_NOT_FOUND;
                },
                [](const HhdmLayoutNotFound &) noexcept { return Reason::HHDM_LAYOUT_NOT_FOUND; },
                [](const ReservedLayoutNotFound &) noexcept {
                    return Reason::RESERVED_LAYOUT_NOT_FOUND;
                },
                [](const HhdmCoverageMissing &) noexcept { return Reason::HHDM_COVERAGE_MISSING; },
                [](const ReservationOwnedByKernel &) noexcept {
                    return Reason::RESERVATION_OWNED_BY_KERNEL;
                },
                [](const ReservationsPresent &) noexcept { return Reason::RESERVATIONS_PRESENT; },
                [](const OwnershipMismatch &) noexcept { return Reason::OWNERSHIP_MISMATCH; },
                [](const NodeAllocationFailed &) noexcept {
                    return Reason::NODE_ALLOCATION_FAILED;
                },
                [](const PagingFailed &) noexcept { return Reason::PAGING_FAILED; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InitializationAlreadyAttempted &) noexcept {
                    return "KernelMM initialization was already attempted";
                },
                [](const DependencyNotReady &) noexcept {
                    return "KernelMM initialization dependency is not ready";
                },
                [](const InvalidKernelLayout &) noexcept { return "invalid kernel layout"; },
                [](const InvalidHhdmLayout &) noexcept { return "invalid HHDM layout"; },
                [](const InvalidReservedLayout &) noexcept { return "invalid reserved layout"; },
                [](const LayoutConflict &) noexcept { return "kernel layouts conflict"; },
                [](const HhdmConflict &) noexcept { return "HHDM layouts conflict"; },
                [](const KernelLayoutNotFound &) noexcept { return "kernel layout was not found"; },
                [](const HhdmLayoutNotFound &) noexcept { return "HHDM layout was not found"; },
                [](const ReservedLayoutNotFound &) noexcept {
                    return "reserved HHDM layout was not found";
                },
                [](const HhdmCoverageMissing &) noexcept {
                    return "physical range is not covered by HHDM";
                },
                [](const ReservationOwnedByKernel &) noexcept {
                    return "reserved HHDM layout is owned by a kernel layout";
                },
                [](const ReservationsPresent &) noexcept {
                    return "HHDM layout still contains reservations";
                },
                [](const OwnershipMismatch &) noexcept {
                    return "kernel layout ownership state is inconsistent";
                },
                [](const NodeAllocationFailed &) noexcept {
                    return "kernel layout node allocation failed";
                },
                [](const PagingFailed &) noexcept {
                    return "kernel layout paging operation failed";
                },
            });
        }

    private:
        using Storage =
            tay::variant<InitializationAlreadyAttempted, DependencyNotReady, InvalidKernelLayout,
                         InvalidHhdmLayout, InvalidReservedLayout, LayoutConflict, HhdmConflict,
                         KernelLayoutNotFound, HhdmLayoutNotFound, ReservedLayoutNotFound,
                         HhdmCoverageMissing, ReservationOwnedByKernel, ReservationsPresent,
                         OwnershipMismatch, NodeAllocationFailed, PagingFailed>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        KernelLayoutError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(KernelLayoutError) <= 56);
    static_assert(std::is_nothrow_move_constructible_v<KernelLayoutError>);
}  // namespace memory
