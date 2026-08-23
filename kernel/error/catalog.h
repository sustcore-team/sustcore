/**
 * @file catalog.h
 * @brief 定义固件目录构造、容量和引用关系错误。
 */

#pragma once

#include <device/fw.h>
#include <error.h>
#include <sustcore/addr.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace device {
    class CatalogError final {
    public:
        using kernel_domain_error_tag = void;

        enum class EntryKind : u8_t {
            DEVICE,
            CPU,
            IRQ_CTRL,
        };

        struct InvalidDescriptor {
            FwId id{};
        };
        struct DuplicateFirmwareId {
            FwId id{};
        };
        struct DuplicateLogicalCpu {
            u32_t cpu_id = 0;
        };
        struct DuplicateHardwareCpu {
            u64_t hw_id = 0;
        };
        struct ParentNotFound {
            FwId id{};
            FwId parent{};
        };
        struct ResourceOverlap {
            FwId id{};
            PhyArea requested{};
            PhyArea existing{};
        };
        struct IrqCtrlNotFound {
            FwId id{};
            FwId controller{};
        };
        struct CapacityExhausted {
            EntryKind entry = EntryKind::DEVICE;
        };
        struct NoCpuDiscovered {};
        struct BackendFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct OutOfMemory {};

        CatalogError()                                    = delete;
        CatalogError(const CatalogError &)                = default;
        CatalogError &operator=(const CatalogError &)     = default;
        CatalogError(CatalogError &&) noexcept            = default;
        CatalogError &operator=(CatalogError &&) noexcept = default;
        ~CatalogError() noexcept                          = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::CatalogError;
            return visit(tay::overloaded{
                [](const InvalidDescriptor &) noexcept { return Reason::INVALID_DESCRIPTOR; },
                [](const DuplicateFirmwareId &) noexcept { return Reason::DUPLICATE_FIRMWARE_ID; },
                [](const DuplicateLogicalCpu &) noexcept { return Reason::DUPLICATE_LOGICAL_CPU; },
                [](const DuplicateHardwareCpu &) noexcept {
                    return Reason::DUPLICATE_HARDWARE_CPU;
                },
                [](const ParentNotFound &) noexcept { return Reason::PARENT_NOT_FOUND; },
                [](const ResourceOverlap &) noexcept { return Reason::RESOURCE_OVERLAP; },
                [](const IrqCtrlNotFound &) noexcept { return Reason::IRQ_CTRL_NOT_FOUND; },
                [](const CapacityExhausted &) noexcept { return Reason::CAPACITY_EXHAUSTED; },
                [](const NoCpuDiscovered &) noexcept { return Reason::NO_CPU_DISCOVERED; },
                [](const BackendFailed &) noexcept { return Reason::BACKEND_FAILED; },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidDescriptor &) noexcept { return "invalid firmware descriptor"; },
                [](const DuplicateFirmwareId &) noexcept {
                    return "duplicate firmware identifier";
                },
                [](const DuplicateLogicalCpu &) noexcept {
                    return "duplicate logical CPU identifier";
                },
                [](const DuplicateHardwareCpu &) noexcept {
                    return "duplicate hardware CPU identifier";
                },
                [](const ParentNotFound &) noexcept { return "firmware parent was not found"; },
                [](const ResourceOverlap &) noexcept { return "firmware resources overlap"; },
                [](const IrqCtrlNotFound &) noexcept {
                    return "interrupt controller was not found";
                },
                [](const CapacityExhausted &) noexcept {
                    return "firmware catalog capacity exhausted";
                },
                [](const NoCpuDiscovered &) noexcept { return "firmware catalog contains no CPU"; },
                [](const BackendFailed &) noexcept {
                    return "firmware enumeration backend failed";
                },
                [](const OutOfMemory &) noexcept { return "firmware catalog allocation failed"; },
            });
        }

    private:
        using Storage =
            tay::variant<InvalidDescriptor, DuplicateFirmwareId, DuplicateLogicalCpu,
                         DuplicateHardwareCpu, ParentNotFound, ResourceOverlap, IrqCtrlNotFound,
                         CapacityExhausted, NoCpuDiscovered, BackendFailed, OutOfMemory>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        CatalogError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(CatalogError) <= 64);
    static_assert(std::is_nothrow_move_constructible_v<CatalogError>);
}  // namespace device
