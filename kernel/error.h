/**
 * @file error.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义内核跨领域错误标识与格式化边界。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/bits.h>
#include <tay/err.h>
#include <tay/format.h>
#include <tay/optional.h>

#include <concepts>
#include <type_traits>

#define KERNEL_TAY_ERROR_REASONS(X) \
    X(OVERFLOW_ERROR)               \
    X(UNDERFLOW_ERROR)              \
    X(OUT_OF_RANGE)                 \
    X(NULLPTR)                      \
    X(INVALID_ARGUMENT)             \
    X(OUT_OF_MEMORY)                \
    X(ALLOCATION_SIZE_OVERFLOW)     \
    X(INTERNAL)

#define KERNEL_CAP_ERROR_REASONS(X) \
    X(INVALID_TOKEN)                \
    X(MISSING_CNODE)                \
    X(INVALID_SLOT)                 \
    X(STALE_TOKEN)                  \
    X(TYPE_MISMATCH)                \
    X(INSUFFICIENT_RIGHTS)          \
    X(NO_SLOTS)                     \
    X(BUSY)                         \
    X(OUT_OF_MEMORY)                \
    X(OPERATION_REJECTED)

#define KERNEL_PAGING_ERROR_REASONS(X) \
    X(INVALID_ROOT)                    \
    X(INVALID_OWNER)                   \
    X(INVALID_STATE)                   \
    X(IDENTIFIER_EXHAUSTED)            \
    X(NON_CANONICAL_ADDRESS)           \
    X(UNALIGNED_RANGE)                 \
    X(RANGE_OVERFLOW)                  \
    X(OUTSIDE_ADDRESS_DOMAIN)          \
    X(INVALID_PHYSICAL_ADDRESS)        \
    X(INVALID_FLAGS)                   \
    X(MISSING_MAPPING)                 \
    X(ALREADY_MAPPED)                  \
    X(UNEXPECTED_ENTRY)                \
    X(UNSUPPORTED_LEAF_LEVEL)          \
    X(PT_ALLOC_FAILED)                 \
    X(OUT_OF_MEMORY)

#define KERNEL_MEMORY_SEGMENT_ERROR_REASONS(X) \
    X(ZERO_SIZE)                               \
    X(SIZE_OVERFLOW)                           \
    X(OFFSET_OUT_OF_RANGE)                     \
    X(PAGE_NOT_ALLOCATED)                      \
    X(INVALID_SOURCE_BUFFER)                   \
    X(PHYS_ALLOC_FAILED)                       \
    X(PAGE_INSERT_FAILED)                      \
    X(OUT_OF_MEMORY)

#define KERNEL_ADDRESS_SPACE_ERROR_REASONS(X) \
    X(INVALID_SEGMENT)                        \
    X(INVALID_AREA)                           \
    X(INVALID_FLAGS)                          \
    X(SEG_OFFSET_OUT_OF_RANGE)                \
    X(MAPPING_EXCEEDS_SEGMENT)                \
    X(ACCESS_DENIED)                          \
    X(VMA_OVERLAP)                            \
    X(VMA_NOT_OWNED)                          \
    X(UNMAPPED_ADDRESS)                       \
    X(MAPPING_CHANGED)                        \
    X(BACKING_ALLOCATION_FAILED)              \
    X(PAGE_TABLE_FAILED)                      \
    X(OUT_OF_MEMORY)

#define KERNEL_LAYOUT_ERROR_REASONS(X) \
    X(INIT_ALREADY_ATTEMPTED)          \
    X(DEPENDENCY_NOT_READY)            \
    X(INVALID_KERNEL_LAYOUT)           \
    X(INVALID_HHDM_LAYOUT)             \
    X(INVALID_RESERVED_LAYOUT)         \
    X(LAYOUT_CONFLICT)                 \
    X(HHDM_CONFLICT)                   \
    X(KERNEL_LAYOUT_NOT_FOUND)         \
    X(HHDM_LAYOUT_NOT_FOUND)           \
    X(RESERVED_LAYOUT_NOT_FOUND)       \
    X(HHDM_COVERAGE_MISSING)           \
    X(RESERVATION_OWNED_BY_KERNEL)     \
    X(RESERVATIONS_PRESENT)            \
    X(OWNERSHIP_MISMATCH)              \
    X(NODE_ALLOCATION_FAILED)          \
    X(PAGING_FAILED)

#define KERNEL_PROCESS_ERROR_REASONS(X) \
    X(OUT_OF_MEMORY)                    \
    X(INVALID_STATE)                    \
    X(KERNEL_PROCESS_OPERATION)         \
    X(ADDRESS_SPACE_ALREADY_SET)        \
    X(CSPACE_ALREADY_SET)               \
    X(MISSING_ADDRESS_SPACE)            \
    X(MISSING_CSPACE)                   \
    X(ALREADY_SUBMITTED)

#define KERNEL_THREAD_ERROR_REASONS(X) \
    X(INVALID_STACK_SIZE)              \
    X(HEAP_UNAVAILABLE)                \
    X(INVALID_ENTRY)                   \
    X(INVALID_USER_STACK)              \
    X(INVALID_MODE)                    \
    X(INVALID_PROCESS_STATE)           \
    X(ALREADY_CONFIGURED)              \
    X(SCHEDULER_ATTACHED)              \
    X(NOT_CURRENT_THREAD)              \
    X(INVALID_THREAD_STATE)            \
    X(TIMED_WAIT_ACTIVE)               \
    X(PIN_FAILED)                      \
    X(TIMER_ARM_FAILED)                \
    X(OUT_OF_MEMORY)

#define KERNEL_SCHEDULER_ERROR_REASONS(X) \
    X(NOT_READY)                          \
    X(ALREADY_READY)                      \
    X(BOOTSTRAP_NOT_RUNNING)              \
    X(INTERRUPTS_ENABLED)                 \
    X(THREAD_ALREADY_ATTACHED)            \
    X(THREAD_NOT_ATTACHED)                \
    X(THREAD_NOT_CONFIGURED)              \
    X(THREAD_NOT_SUBMITTED)               \
    X(INVALID_THREAD_STATE)               \
    X(QUEUE_STATE_MISMATCH)               \
    X(TIMED_WAIT_ACTIVE)                  \
    X(INVALID_BLOCK_TOKEN)                \
    X(NO_RUNNABLE_THREAD)                 \
    X(IDLE_THREAD_OPERATION)              \
    X(PREEMPT_SINK_ALREADY_SET)           \
    X(INVALID_DEADLINE_SINK)

#define KERNEL_TIMER_ERROR_REASONS(X) \
    X(WORK_QUEUE_NOT_ACCEPTING)       \
    X(ENGINE_NOT_INITIALIZED)         \
    X(NODE_NOT_IDLE)                  \
    X(NODE_ALREADY_LINKED)            \
    X(COMPLETION_BUSY)

#define KERNEL_WORK_QUEUE_ERROR_REASONS(X) \
    X(ALREADY_STARTED)                     \
    X(PENDING_WORK)                        \
    X(WORKER_CREATE_FAILED)                \
    X(WORKER_ATTACH_FAILED)                \
    X(NOT_STARTED)                         \
    X(PERMANENT_QUEUE)                     \
    X(CALLED_BY_WORKER)

#define KERNEL_USRBOOT_ERROR_REASONS(X) \
    X(IMAGE_TOO_SMALL)                  \
    X(INVALID_MAGIC)                    \
    X(INVALID_HEADER)                   \
    X(INVALID_SEGMENT_SIZE)             \
    X(SEG_ADDR_OVERFLOW)                \
    X(SEGMENT_OUTSIDE_USER_RANGE)       \
    X(SEGMENT_FILE_RANGE_INVALID)       \
    X(SEGMENT_UNALIGNED)                \
    X(OBJECT_CREATE_FAILED)             \
    X(PROCESS_CONFIG_FAILED)            \
    X(VMA_CREATION_FAILED)              \
    X(SEGMENT_WRITE_FAILED)             \
    X(INITIAL_STACK_FAILED)             \
    X(THREAD_CREATION_FAILED)           \
    X(USER_CTX_CONFIG_FAILED)           \
    X(PROCESS_SUBMISSION_FAILED)        \
    X(THREAD_ATTACH_FAILED)

#define KERNEL_CATALOG_ERROR_REASONS(X) \
    X(INVALID_DESCRIPTOR)               \
    X(DUPLICATE_FIRMWARE_ID)            \
    X(DUPLICATE_LOGICAL_CPU)            \
    X(DUPLICATE_HARDWARE_CPU)           \
    X(PARENT_NOT_FOUND)                 \
    X(RESOURCE_OVERLAP)                 \
    X(IRQ_CTRL_NOT_FOUND)               \
    X(CAPACITY_EXHAUSTED)               \
    X(NO_CPU_DISCOVERED)                \
    X(BACKEND_FAILED)                   \
    X(OUT_OF_MEMORY)

#define KERNEL_FDT_ERROR_REASONS(X) \
    X(INVALID_BLOB)                 \
    X(NODE_NOT_FOUND)               \
    X(MISSING_PROPERTY)             \
    X(INVALID_PROPERTY)             \
    X(CELL_COUNT_UNSUPPORTED)       \
    X(ADDR_TRANSLATE_FAILED)        \
    X(INTEGER_OVERFLOW)             \
    X(BOOT_CPU_NOT_FOUND)           \
    X(CATALOG_REJECTED)

#define KERNEL_MMIO_ERROR_REASONS(X) \
    X(INVALID_PHYSICAL_AREA)         \
    X(SIZE_OVERFLOW)                 \
    X(MAPPING_CONFLICT)              \
    X(KERNEL_SPACE_UNAVAILABLE)      \
    X(NOT_MAPPED)                    \
    X(PAGING_FAILED)                 \
    X(OUT_OF_MEMORY)

#define KERNEL_PLIC_ERROR_REASONS(X) \
    X(INVALID_MMIO_RANGE)            \
    X(CONTEXT_OUT_OF_RANGE)          \
    X(SOURCE_OUT_OF_RANGE)           \
    X(INVALID_PRIORITY)              \
    X(MISSING_MMIO)                  \
    X(MISSING_CONTEXT)               \
    X(INVALID_CLAIM)                 \
    X(CONTROLLER_NOT_FOUND)          \
    X(MMIO_FAILED)                   \
    X(PLIC_ALLOCATION_FAILED)        \
    X(DOMAIN_REGISTRATION_FAILED)

namespace kernel {
    /**
     * @brief 以 `Type(Reason)` 形式保存可跨子系统传递的精确错误标识。
     *
     * 结构化 payload 仍由各领域 error wrapper 保存；转换为 `KernelError` 时只保留稳定的
     * 领域类型与 alternative reason，不暴露 variant 布局。
     */
    class KernelError final {
    public:
        enum class Type : u8_t {
            TAY_ERROR,
            CAP_ERROR,
            PAGING_ERROR,
            MEMORY_SEGMENT_ERROR,
            ADDRESS_SPACE_ERROR,
            KERNEL_LAYOUT_ERROR,
            PROCESS_ERROR,
            THREAD_ERROR,
            SCHEDULER_ERROR,
            TIMER_ERROR,
            WORK_QUEUE_ERROR,
            USRBOOT_ERROR,
            CATALOG_ERROR,
            FDT_ERROR,
            MMIO_ERROR,
            PLIC_ERROR,
        };

#define KERNEL_DECLARE_REASON(name) name,
        enum class TayError : u8_t { KERNEL_TAY_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class CError : u8_t { KERNEL_CAP_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class PagingError : u8_t { KERNEL_PAGING_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class MemSegError : u8_t {
            KERNEL_MEMORY_SEGMENT_ERROR_REASONS(KERNEL_DECLARE_REASON)
        };
        enum class AddrSpaceError : u8_t {
            KERNEL_ADDRESS_SPACE_ERROR_REASONS(KERNEL_DECLARE_REASON)
        };
        enum class KernelMapError : u8_t { KERNEL_LAYOUT_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class ProcessError : u8_t { KERNEL_PROCESS_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class ThreadError : u8_t { KERNEL_THREAD_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class SchedulerError : u8_t { KERNEL_SCHEDULER_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class TimerError : u8_t { KERNEL_TIMER_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class WorkQueueError : u8_t { KERNEL_WORK_QUEUE_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class UsrbootError : u8_t { KERNEL_USRBOOT_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class CatalogError : u8_t { KERNEL_CATALOG_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class FdtError : u8_t { KERNEL_FDT_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class MmioError : u8_t { KERNEL_MMIO_ERROR_REASONS(KERNEL_DECLARE_REASON) };
        enum class PlicError : u8_t { KERNEL_PLIC_ERROR_REASONS(KERNEL_DECLARE_REASON) };
#undef KERNEL_DECLARE_REASON

#define KERNEL_REASON_CONSTRUCTOR(reason_type, type_value) \
    constexpr KernelError(reason_type reason) noexcept     \
        : type_(Type::type_value), reason_(static_cast<u8_t>(reason)) {}
        KERNEL_REASON_CONSTRUCTOR(TayError, TAY_ERROR)
        KERNEL_REASON_CONSTRUCTOR(CError, CAP_ERROR)
        KERNEL_REASON_CONSTRUCTOR(PagingError, PAGING_ERROR)
        KERNEL_REASON_CONSTRUCTOR(MemSegError, MEMORY_SEGMENT_ERROR)
        KERNEL_REASON_CONSTRUCTOR(AddrSpaceError, ADDRESS_SPACE_ERROR)
        KERNEL_REASON_CONSTRUCTOR(KernelMapError, KERNEL_LAYOUT_ERROR)
        KERNEL_REASON_CONSTRUCTOR(ProcessError, PROCESS_ERROR)
        KERNEL_REASON_CONSTRUCTOR(ThreadError, THREAD_ERROR)
        KERNEL_REASON_CONSTRUCTOR(SchedulerError, SCHEDULER_ERROR)
        KERNEL_REASON_CONSTRUCTOR(TimerError, TIMER_ERROR)
        KERNEL_REASON_CONSTRUCTOR(WorkQueueError, WORK_QUEUE_ERROR)
        KERNEL_REASON_CONSTRUCTOR(UsrbootError, USRBOOT_ERROR)
        KERNEL_REASON_CONSTRUCTOR(CatalogError, CATALOG_ERROR)
        KERNEL_REASON_CONSTRUCTOR(FdtError, FDT_ERROR)
        KERNEL_REASON_CONSTRUCTOR(MmioError, MMIO_ERROR)
        KERNEL_REASON_CONSTRUCTOR(PlicError, PLIC_ERROR)
#undef KERNEL_REASON_CONSTRUCTOR

        [[nodiscard]] constexpr Type type() const noexcept {
            return type_;
        }

        template <typename Reason>
        [[nodiscard]] constexpr bool is() const noexcept {
            return type_ == type_of<Reason>();
        }

        template <typename Reason>
        [[nodiscard]] constexpr tay::optional<Reason> reason() const noexcept {
            if (!is<Reason>())
                return tay::nullopt;
            return static_cast<Reason>(reason_);
        }

        [[nodiscard]] constexpr const char *type_name() const noexcept {
            return name(type_);
        }

        [[nodiscard]] constexpr const char *reason_name() const noexcept {
            switch (type_) {
                case Type::TAY_ERROR:            return name(static_cast<TayError>(reason_));
                case Type::CAP_ERROR:            return name(static_cast<CError>(reason_));
                case Type::PAGING_ERROR:         return name(static_cast<PagingError>(reason_));
                case Type::MEMORY_SEGMENT_ERROR: return name(static_cast<MemSegError>(reason_));
                case Type::ADDRESS_SPACE_ERROR:  return name(static_cast<AddrSpaceError>(reason_));
                case Type::KERNEL_LAYOUT_ERROR:  return name(static_cast<KernelMapError>(reason_));
                case Type::PROCESS_ERROR:        return name(static_cast<ProcessError>(reason_));
                case Type::THREAD_ERROR:         return name(static_cast<ThreadError>(reason_));
                case Type::SCHEDULER_ERROR:      return name(static_cast<SchedulerError>(reason_));
                case Type::TIMER_ERROR:          return name(static_cast<TimerError>(reason_));
                case Type::WORK_QUEUE_ERROR:     return name(static_cast<WorkQueueError>(reason_));
                case Type::USRBOOT_ERROR:        return name(static_cast<UsrbootError>(reason_));
                case Type::CATALOG_ERROR:        return name(static_cast<CatalogError>(reason_));
                case Type::FDT_ERROR:            return name(static_cast<FdtError>(reason_));
                case Type::MMIO_ERROR:           return name(static_cast<MmioError>(reason_));
                case Type::PLIC_ERROR:           return name(static_cast<PlicError>(reason_));
            }
            return "UNKNOWN";
        }

        [[nodiscard]] static constexpr const char *name(Type type) noexcept {
            switch (type) {
                case Type::TAY_ERROR:            return "TayError";
                case Type::CAP_ERROR:            return "CError";
                case Type::PAGING_ERROR:         return "PagingError";
                case Type::MEMORY_SEGMENT_ERROR: return "MemSegError";
                case Type::ADDRESS_SPACE_ERROR:  return "AddrSpaceError";
                case Type::KERNEL_LAYOUT_ERROR:  return "KernelMapError";
                case Type::PROCESS_ERROR:        return "ProcessError";
                case Type::THREAD_ERROR:         return "ThreadError";
                case Type::SCHEDULER_ERROR:      return "SchedulerError";
                case Type::TIMER_ERROR:          return "TimerError";
                case Type::WORK_QUEUE_ERROR:     return "WorkQueueError";
                case Type::USRBOOT_ERROR:        return "UsrbootError";
                case Type::CATALOG_ERROR:        return "CatalogError";
                case Type::FDT_ERROR:            return "FdtError";
                case Type::MMIO_ERROR:           return "MmioError";
                case Type::PLIC_ERROR:           return "PlicError";
            }
            return "UnknownError";
        }

#define KERNEL_REASON_NAME_CASE(name) \
    case name: return #name;
#define KERNEL_DECLARE_REASON_NAME(reason_type, reasons)                           \
    [[nodiscard]] static constexpr const char *name(reason_type reason) noexcept { \
        using enum reason_type;                                                    \
        switch (reason) {                                                          \
            reasons(KERNEL_REASON_NAME_CASE)                                       \
        }                                                                          \
        return "UNKNOWN";                                                          \
    }
        KERNEL_DECLARE_REASON_NAME(TayError, KERNEL_TAY_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(CError, KERNEL_CAP_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(PagingError, KERNEL_PAGING_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(MemSegError, KERNEL_MEMORY_SEGMENT_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(AddrSpaceError, KERNEL_ADDRESS_SPACE_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(KernelMapError, KERNEL_LAYOUT_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(ProcessError, KERNEL_PROCESS_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(ThreadError, KERNEL_THREAD_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(SchedulerError, KERNEL_SCHEDULER_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(TimerError, KERNEL_TIMER_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(WorkQueueError, KERNEL_WORK_QUEUE_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(UsrbootError, KERNEL_USRBOOT_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(CatalogError, KERNEL_CATALOG_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(FdtError, KERNEL_FDT_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(MmioError, KERNEL_MMIO_ERROR_REASONS)
        KERNEL_DECLARE_REASON_NAME(PlicError, KERNEL_PLIC_ERROR_REASONS)
#undef KERNEL_DECLARE_REASON_NAME
#undef KERNEL_REASON_NAME_CASE

        friend constexpr bool operator==(KernelError, KernelError) noexcept = default;

    private:
        template <typename Reason>
        [[nodiscard]] static consteval Type type_of() noexcept {
            if constexpr (std::same_as<Reason, TayError>)
                return Type::TAY_ERROR;
            else if constexpr (std::same_as<Reason, CError>)
                return Type::CAP_ERROR;
            else if constexpr (std::same_as<Reason, PagingError>)
                return Type::PAGING_ERROR;
            else if constexpr (std::same_as<Reason, MemSegError>)
                return Type::MEMORY_SEGMENT_ERROR;
            else if constexpr (std::same_as<Reason, AddrSpaceError>)
                return Type::ADDRESS_SPACE_ERROR;
            else if constexpr (std::same_as<Reason, KernelMapError>)
                return Type::KERNEL_LAYOUT_ERROR;
            else if constexpr (std::same_as<Reason, ProcessError>)
                return Type::PROCESS_ERROR;
            else if constexpr (std::same_as<Reason, ThreadError>)
                return Type::THREAD_ERROR;
            else if constexpr (std::same_as<Reason, SchedulerError>)
                return Type::SCHEDULER_ERROR;
            else if constexpr (std::same_as<Reason, TimerError>)
                return Type::TIMER_ERROR;
            else if constexpr (std::same_as<Reason, WorkQueueError>)
                return Type::WORK_QUEUE_ERROR;
            else if constexpr (std::same_as<Reason, UsrbootError>)
                return Type::USRBOOT_ERROR;
            else if constexpr (std::same_as<Reason, CatalogError>)
                return Type::CATALOG_ERROR;
            else if constexpr (std::same_as<Reason, FdtError>)
                return Type::FDT_ERROR;
            else if constexpr (std::same_as<Reason, MmioError>)
                return Type::MMIO_ERROR;
            else {
                static_assert(std::same_as<Reason, PlicError>, "not a KernelError reason type");
                return Type::PLIC_ERROR;
            }
        }

        Type type_;
        u8_t reason_;
    };

    template <typename T>
    concept KernelErrorReason =
        std::same_as<T, KernelError::TayError> || std::same_as<T, KernelError::CError> ||
        std::same_as<T, KernelError::PagingError> || std::same_as<T, KernelError::MemSegError> ||
        std::same_as<T, KernelError::AddrSpaceError> ||
        std::same_as<T, KernelError::KernelMapError> ||
        std::same_as<T, KernelError::ProcessError> || std::same_as<T, KernelError::ThreadError> ||
        std::same_as<T, KernelError::SchedulerError> || std::same_as<T, KernelError::TimerError> ||
        std::same_as<T, KernelError::WorkQueueError> ||
        std::same_as<T, KernelError::UsrbootError> || std::same_as<T, KernelError::CatalogError> ||
        std::same_as<T, KernelError::FdtError> || std::same_as<T, KernelError::MmioError> ||
        std::same_as<T, KernelError::PlicError>;

    template <typename T>
    concept DomainError = requires(const T &error) {
        typename T::kernel_domain_error_tag;
        {
            error.code()
        } -> std::same_as<KernelError>;
    };

    [[nodiscard]] constexpr tay::optional<KernelError> from_tay_error(
        tay::error_code error) noexcept {
        using Reason = KernelError::TayError;
        switch (error) {
            case tay::error_code::NONE:                     return tay::nullopt;
            case tay::error_code::OVERFLOW_ERROR:           return Reason::OVERFLOW_ERROR;
            case tay::error_code::UNDERFLOW_ERROR:          return Reason::UNDERFLOW_ERROR;
            case tay::error_code::OUT_OF_RANGE:             return Reason::OUT_OF_RANGE;
            case tay::error_code::NULLPTR:                  return Reason::NULLPTR;
            case tay::error_code::INVALID_ARGUMENT:         return Reason::INVALID_ARGUMENT;
            case tay::error_code::OUT_OF_MEMORY:            return Reason::OUT_OF_MEMORY;
            case tay::error_code::ALLOCATION_SIZE_OVERFLOW: return Reason::ALLOCATION_SIZE_OVERFLOW;
        }
        return Reason::INTERNAL;
    }

    /** @brief 在尚未迁移的宽边界显式丢弃领域细节并恢复 Tay 基础错误。 */
    [[nodiscard]] constexpr tay::error_code to_tay_error(KernelError error) noexcept {
        using Type = KernelError::Type;
        if (error.type() == Type::TAY_ERROR) {
            switch (*error.reason<KernelError::TayError>()) {
                case KernelError::TayError::OVERFLOW_ERROR:
                case KernelError::TayError::UNDERFLOW_ERROR:
                case KernelError::TayError::OUT_OF_RANGE:    return tay::error_code::OUT_OF_RANGE;
                case KernelError::TayError::OUT_OF_MEMORY:
                case KernelError::TayError::ALLOCATION_SIZE_OVERFLOW:
                    return tay::error_code::OUT_OF_MEMORY;
                default: return tay::error_code::INVALID_ARGUMENT;
            }
        }
        if (error.type() == Type::PLIC_ERROR) {
            const auto reason = *error.reason<KernelError::PlicError>();
            if (reason == KernelError::PlicError::PLIC_ALLOCATION_FAILED)
                return tay::error_code::OUT_OF_MEMORY;
            if (reason == KernelError::PlicError::CONTEXT_OUT_OF_RANGE ||
                reason == KernelError::PlicError::SOURCE_OUT_OF_RANGE ||
                reason == KernelError::PlicError::MISSING_MMIO ||
                reason == KernelError::PlicError::MISSING_CONTEXT ||
                reason == KernelError::PlicError::CONTROLLER_NOT_FOUND)
                return tay::error_code::OUT_OF_RANGE;
        }
        if (error.type() == Type::FDT_ERROR) {
            const auto reason = *error.reason<KernelError::FdtError>();
            if (reason == KernelError::FdtError::NODE_NOT_FOUND ||
                reason == KernelError::FdtError::MISSING_PROPERTY ||
                reason == KernelError::FdtError::BOOT_CPU_NOT_FOUND)
                return tay::error_code::OUT_OF_RANGE;
        }
        if (error.type() == Type::ADDRESS_SPACE_ERROR) {
            const auto reason = *error.reason<KernelError::AddrSpaceError>();
            if (reason == KernelError::AddrSpaceError::BACKING_ALLOCATION_FAILED ||
                reason == KernelError::AddrSpaceError::OUT_OF_MEMORY)
                return tay::error_code::OUT_OF_MEMORY;
            if (reason == KernelError::AddrSpaceError::SEG_OFFSET_OUT_OF_RANGE ||
                reason == KernelError::AddrSpaceError::MAPPING_EXCEEDS_SEGMENT ||
                reason == KernelError::AddrSpaceError::VMA_NOT_OWNED ||
                reason == KernelError::AddrSpaceError::UNMAPPED_ADDRESS)
                return tay::error_code::OUT_OF_RANGE;
        }
        return tay::error_code::INVALID_ARGUMENT;
    }

    static_assert(sizeof(KernelError) == 2);
    static_assert(std::is_trivially_copyable_v<KernelError>);
}  // namespace kernel

namespace tay {
    template <>
    struct formatter<kernel::KernelError::Type> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(kernel::KernelError::Type type,
                                                FormatContext &context) const {
            context.write(kernel::KernelError::name(type));
            return context.out();
        }
    };

    template <kernel::KernelErrorReason Reason>
    struct formatter<Reason> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(Reason reason, FormatContext &context) const {
            context.write(kernel::KernelError::name(reason));
            return context.out();
        }
    };

    template <>
    struct formatter<kernel::KernelError> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(kernel::KernelError error,
                                                FormatContext &context) const {
            context.write(error.type_name());
            context.put('(');
            context.write(error.reason_name());
            context.put(')');
            return context.out();
        }
    };

    template <kernel::DomainError Error>
    struct formatter<Error> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(const Error &error, FormatContext &context) const {
            context.format("{}", error.code());
            return context.out();
        }
    };
}  // namespace tay

#undef KERNEL_TAY_ERROR_REASONS
#undef KERNEL_CAP_ERROR_REASONS
#undef KERNEL_PAGING_ERROR_REASONS
#undef KERNEL_MEMORY_SEGMENT_ERROR_REASONS
#undef KERNEL_ADDRESS_SPACE_ERROR_REASONS
#undef KERNEL_LAYOUT_ERROR_REASONS
#undef KERNEL_PROCESS_ERROR_REASONS
#undef KERNEL_THREAD_ERROR_REASONS
#undef KERNEL_SCHEDULER_ERROR_REASONS
#undef KERNEL_TIMER_ERROR_REASONS
#undef KERNEL_WORK_QUEUE_ERROR_REASONS
#undef KERNEL_USRBOOT_ERROR_REASONS
#undef KERNEL_CATALOG_ERROR_REASONS
#undef KERNEL_FDT_ERROR_REASONS
#undef KERNEL_MMIO_ERROR_REASONS
#undef KERNEL_PLIC_ERROR_REASONS
