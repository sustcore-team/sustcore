/**
 * @file fdt.h
 * @brief 定义永久内核 FDT 解析与目录投影错误。
 */

#pragma once

#include <error.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace device {
    enum class PropertyId : u8_t {
        ADDRESS_CELLS,
        SIZE_CELLS,
        REG,
        TIMEBASE_FREQUENCY,
        DEVICE_TYPE,
        COMPATIBLE,
        INTERRUPT_PARENT,
        INTERRUPT_CELLS,
        RANGES,
    };

    class FdtError final {
    public:
        using kernel_domain_error_tag = void;

        struct InvalidBlob {};
        struct NodeNotFound {
            u32_t node_offset = 0;
        };
        struct MissingProperty {
            u32_t node_offset   = 0;
            PropertyId property = PropertyId::REG;
        };
        struct InvalidProperty {
            u32_t node_offset   = 0;
            PropertyId property = PropertyId::REG;
        };
        struct CellCountUnsupported {
            u32_t node_offset  = 0;
            u8_t address_cells = 0;
            u8_t size_cells    = 0;
        };
        struct AddrTranslateFailed {
            u32_t node_offset = 0;
        };
        struct IntegerOverflow {
            u32_t node_offset   = 0;
            PropertyId property = PropertyId::REG;
        };
        struct BootCpuNotFound {
            u64_t hw_id = 0;
        };
        struct CatalogRejected {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };

        FdtError()                                = delete;
        FdtError(const FdtError &)                = default;
        FdtError &operator=(const FdtError &)     = default;
        FdtError(FdtError &&) noexcept            = default;
        FdtError &operator=(FdtError &&) noexcept = default;
        ~FdtError() noexcept                      = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::FdtError;
            return visit(tay::overloaded{
                [](const InvalidBlob &) noexcept { return Reason::INVALID_BLOB; },
                [](const NodeNotFound &) noexcept { return Reason::NODE_NOT_FOUND; },
                [](const MissingProperty &) noexcept { return Reason::MISSING_PROPERTY; },
                [](const InvalidProperty &) noexcept { return Reason::INVALID_PROPERTY; },
                [](const CellCountUnsupported &) noexcept {
                    return Reason::CELL_COUNT_UNSUPPORTED;
                },
                [](const AddrTranslateFailed &) noexcept { return Reason::ADDR_TRANSLATE_FAILED; },
                [](const IntegerOverflow &) noexcept { return Reason::INTEGER_OVERFLOW; },
                [](const BootCpuNotFound &) noexcept { return Reason::BOOT_CPU_NOT_FOUND; },
                [](const CatalogRejected &) noexcept { return Reason::CATALOG_REJECTED; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidBlob &) noexcept { return "invalid FDT blob"; },
                [](const NodeNotFound &) noexcept { return "FDT node was not found"; },
                [](const MissingProperty &) noexcept { return "required FDT property is missing"; },
                [](const InvalidProperty &) noexcept { return "FDT property is invalid"; },
                [](const CellCountUnsupported &) noexcept {
                    return "FDT cell count is unsupported";
                },
                [](const AddrTranslateFailed &) noexcept {
                    return "FDT address translation failed";
                },
                [](const IntegerOverflow &) noexcept { return "FDT integer overflows"; },
                [](const BootCpuNotFound &) noexcept { return "boot CPU is absent from FDT"; },
                [](const CatalogRejected &) noexcept {
                    return "firmware catalog rejected FDT data";
                },
            });
        }

    private:
        using Storage = tay::variant<InvalidBlob, NodeNotFound, MissingProperty, InvalidProperty,
                                     CellCountUnsupported, AddrTranslateFailed, IntegerOverflow,
                                     BootCpuNotFound, CatalogRejected>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        FdtError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    /** @brief 在 FwEnumerator 固定虚接口边界归约错误，并优先保留嵌套 cause。 */
    [[nodiscard]] inline tay::error_code to_tay_error(const FdtError &error) noexcept {
        tay::optional<kernel::KernelError> cause;
        error.visit([&](const auto &value) noexcept {
            if constexpr (requires { value.cause; })
                cause = value.cause;
        });
        return kernel::to_tay_error(cause.value_or(error.code()));
    }

    static_assert(sizeof(FdtError) <= 24);
    static_assert(std::is_nothrow_move_constructible_v<FdtError>);
}  // namespace device
