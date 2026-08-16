/**
 * @file error.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证内核通用错误分类及 Tay 错误映射。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#include <async/work_queue_error.h>
#include <cap/error.h>
#include <device/catalog_error.h>
#include <device/fdt_error.h>
#include <device/mmio_error.h>
#include <error.h>
#include <init/usrboot_error.h>
#include <memory/memory_segment_error.h>
#include <memory/virtual/kernel/kernel_layout_error.h>
#include <memory/virtual/paging_error.h>
#include <scheduler/error.h>
#include <task/address_space_error.h>
#include <task/process_error.h>
#include <task/thread_error.h>
#include <test/cases.h>
#include <timer/error.h>

#if defined(__ARCH_RISCV64__)
#include <arch/riscv64/device/plic_error.h>
#endif

#include <type_traits>

namespace kernel::test::cases {
    namespace {
        constexpr bool maps_to(tay::error_code source, KernelError expected) noexcept {
            const auto mapped = from_tay_error(source);
            return mapped && *mapped == expected;
        }

        static_assert(!from_tay_error(tay::error_code::NONE));
        static_assert(maps_to(tay::error_code::OVERFLOW_ERROR,
                              KernelError::TayError::OVERFLOW_ERROR));
        static_assert(maps_to(tay::error_code::UNDERFLOW_ERROR,
                              KernelError::TayError::UNDERFLOW_ERROR));
        static_assert(maps_to(tay::error_code::OUT_OF_RANGE, KernelError::TayError::OUT_OF_RANGE));
        static_assert(maps_to(tay::error_code::NULLPTR, KernelError::TayError::NULLPTR));
        static_assert(maps_to(tay::error_code::INVALID_ARGUMENT,
                              KernelError::TayError::INVALID_ARGUMENT));
        static_assert(maps_to(tay::error_code::OUT_OF_MEMORY,
                              KernelError::TayError::OUT_OF_MEMORY));
        static_assert(maps_to(tay::error_code::ALLOCATION_SIZE_OVERFLOW,
                              KernelError::TayError::ALLOCATION_SIZE_OVERFLOW));
        constexpr KernelError CAP_INVALID_TOKEN =
            KernelError::CapError(KernelError::CapError::INVALID_TOKEN);
        static_assert(CAP_INVALID_TOKEN.type() == KernelError::Type::CAP_ERROR);
        static_assert(*CAP_INVALID_TOKEN.reason<KernelError::CapError>() ==
                      KernelError::CapError::INVALID_TOKEN);

        [[nodiscard]] constexpr KernelError::Type expected_type(const cap::CapError &) noexcept {
            return KernelError::Type::CAP_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const memory::PagingError &) noexcept {
            return KernelError::Type::PAGING_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const memory::MemorySegmentError &) noexcept {
            return KernelError::Type::MEMORY_SEGMENT_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const task::AddressSpaceError &) noexcept {
            return KernelError::Type::ADDRESS_SPACE_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const memory::KernelLayoutError &) noexcept {
            return KernelError::Type::KERNEL_LAYOUT_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const task::ProcessError &) noexcept {
            return KernelError::Type::PROCESS_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const task::ThreadError &) noexcept {
            return KernelError::Type::THREAD_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const scheduler::SchedulerError &) noexcept {
            return KernelError::Type::SCHEDULER_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const kernel::timer::TimerError &) noexcept {
            return KernelError::Type::TIMER_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const kernel::async::WorkQueueError &) noexcept {
            return KernelError::Type::WORK_QUEUE_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const init::UsrbootError &) noexcept {
            return KernelError::Type::USRBOOT_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const device::CatalogError &) noexcept {
            return KernelError::Type::CATALOG_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(const device::FdtError &) noexcept {
            return KernelError::Type::FDT_ERROR;
        }
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const device::MmioError &) noexcept {
            return KernelError::Type::MMIO_ERROR;
        }
#if defined(__ARCH_RISCV64__)
        [[nodiscard]] constexpr KernelError::Type expected_type(
            const riscv64::device::interrupt::PlicError &) noexcept {
            return KernelError::Type::PLIC_ERROR;
        }
#endif

        struct FormatSink {
            char data[64]{};
            size_t size = 0;

            int operator()(const char *text, size_t length) noexcept {
                if (length > sizeof(data) - size)
                    return -1;
                for (size_t i = 0; i < length; ++i) data[size + i] = text[i];
                size += length;
                return static_cast<int>(length);
            }
        };

        [[nodiscard]] bool equals(const FormatSink &sink, const char *expected) noexcept {
            size_t index = 0;
            while (expected[index] != '\0') {
                if (index >= sink.size || sink.data[index] != expected[index])
                    return false;
                ++index;
            }
            return index == sink.size;
        }

        template <typename Value>
        [[nodiscard]] bool formatter_matches(const Value &value, const char *expected) noexcept {
            FormatSink sink;
            const auto formatted = tay::format_to(sink, "{}", value);
            return formatted && equals(sink, expected);
        }

        [[nodiscard]] bool kernel_formatter_matches(KernelError error) noexcept {
            FormatSink sink;
            const auto formatted = tay::format_to(sink, "{}", error);
            if (!formatted)
                return false;

            size_t index = 0;
            for (const char *part : {error.type_name(), "(", error.reason_name(), ")"}) {
                for (size_t i = 0; part[i] != '\0'; ++i) {
                    if (index >= sink.size || sink.data[index++] != part[i])
                        return false;
                }
            }
            return index == sink.size;
        }

        [[nodiscard]] bool reason_formatter_matches(KernelError error) noexcept {
#define MATCH_REASON(TypeName, ReasonName)                            \
    case KernelError::Type::TypeName: {                               \
        const auto reason = *error.reason<KernelError::ReasonName>(); \
        return formatter_matches(reason, error.reason_name());        \
    }
            switch (error.type()) {
                MATCH_REASON(TAY_ERROR, TayError)
                MATCH_REASON(CAP_ERROR, CapError)
                MATCH_REASON(PAGING_ERROR, PagingError)
                case KernelError::Type::MEMORY_SEGMENT_ERROR: {
                    const auto reason = *error.reason<KernelError::MemorySegmentError>();
                    return formatter_matches(reason, error.reason_name());
                }
                case KernelError::Type::ADDRESS_SPACE_ERROR: {
                    const auto reason = *error.reason<KernelError::AddressSpaceError>();
                    return formatter_matches(reason, error.reason_name());
                }
                case KernelError::Type::KERNEL_LAYOUT_ERROR: {
                    const auto reason = *error.reason<KernelError::KernelLayoutError>();
                    return formatter_matches(reason, error.reason_name());
                }
                    MATCH_REASON(PROCESS_ERROR, ProcessError)
                    MATCH_REASON(THREAD_ERROR, ThreadError)
                    MATCH_REASON(SCHEDULER_ERROR, SchedulerError)
                    MATCH_REASON(TIMER_ERROR, TimerError)
                case KernelError::Type::WORK_QUEUE_ERROR: {
                    const auto reason = *error.reason<KernelError::WorkQueueError>();
                    return formatter_matches(reason, error.reason_name());
                }
                    MATCH_REASON(USRBOOT_ERROR, UsrbootError)
                    MATCH_REASON(CATALOG_ERROR, CatalogError)
                    MATCH_REASON(FDT_ERROR, FdtError)
                    MATCH_REASON(MMIO_ERROR, MmioError)
                    MATCH_REASON(PLIC_ERROR, PlicError)
            }
#undef MATCH_REASON
            return false;
        }

        [[nodiscard]] bool reason_matches_alternative(const char *reason,
                                                      const char *alternative) noexcept {
            while (*reason != '\0' || *alternative != '\0') {
                while (*reason == '_') ++reason;
                if (*reason == '\0' || *alternative == '\0')
                    return *reason == *alternative;
                char expected = *alternative++;
                if (expected >= 'a' && expected <= 'z')
                    expected = static_cast<char>(expected - 'a' + 'A');
                if (*reason++ != expected)
                    return false;
            }
            return true;
        }

        template <typename DomainError, typename Alternative>
        void verify_error(const DomainError &error, const char *alternative,
                          const char *name) noexcept {
            kernel::test::require(error.template is<Alternative>(), name);
            bool visited = false;
            error.visit([&](const auto &value) noexcept {
                using Value = std::remove_cvref_t<decltype(value)>;
                visited     = std::is_same_v<Value, Alternative>;
            });
            kernel::test::require(visited, "领域错误 visit() 返回了错误 alternative");
            const auto code = error.code();
            kernel::test::require(code.type() == expected_type(error),
                                  "领域错误 KernelError type 映射错误");
            kernel::test::require(reason_matches_alternative(code.reason_name(), alternative),
                                  "领域错误 KernelError reason 映射错误");
            kernel::test::require(kernel_formatter_matches(code), "KernelError formatter 输出错误");
            kernel::test::require(reason_formatter_matches(code),
                                  "KernelError reason formatter 输出错误");
            FormatSink domain_sink;
            FormatSink code_sink;
            const auto formatted_domain = tay::format_to(domain_sink, "{}", error);
            const auto formatted_code   = tay::format_to(code_sink, "{}", code);
            bool same_output =
                formatted_domain && formatted_code && domain_sink.size == code_sink.size;
            for (size_t i = 0; same_output && i < domain_sink.size; ++i)
                same_output = domain_sink.data[i] == code_sink.data[i];
            kernel::test::require(same_output, "领域错误 formatter 输出错误");
            kernel::test::require(error.message() != nullptr && error.message()[0] != '\0',
                                  "领域错误 message() 为空");
        }

#define VERIFY_ERROR(Error, Alternative, Factory, Code) \
    verify_error<Error, Error::Alternative>((Factory), #Alternative, #Error "::" #Alternative)

        void verify_p0_errors() noexcept {
            const auto token = cap::encode_token(1, 2, 3, 4);
            VERIFY_ERROR(cap::CapError, InvalidToken, cap::CapError::InvalidToken(token),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(cap::CapError, MissingCNode, cap::CapError::MissingCNode(token, 3),
                         NOT_FOUND);
            VERIFY_ERROR(cap::CapError, InvalidSlot, cap::CapError::InvalidSlot(token, 4),
                         OUT_OF_RANGE);
            const cap::CapError stale = cap::CapError::StaleToken(token, 9);
            VERIFY_ERROR(cap::CapError, StaleToken, stale, STALE);
            bool stale_payload = false;
            stale.visit([&](const auto &value) noexcept {
                using Value = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, cap::CapError::StaleToken>)
                    stale_payload = value.token == token && value.observed_generation == 9;
            });
            kernel::test::require(stale_payload, "StaleToken payload 丢失");
            VERIFY_ERROR(cap::CapError, TypeMismatch,
                         cap::CapError::TypeMismatch(token, cap::ObjectType::THREAD,
                                                     cap::ObjectType::PROCESS),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(
                cap::CapError, InsufficientRights,
                cap::CapError::InsufficientRights(token, cap::RIGHT_WRITE, cap::RIGHT_READ),
                ACCESS_DENIED);
            VERIFY_ERROR(cap::CapError, NoSlots, cap::CapError::NoSlots(2), RESOURCE_EXHAUSTED);
            VERIFY_ERROR(cap::CapError, Busy, cap::CapError::Busy(2), BUSY);
            VERIFY_ERROR(cap::CapError, OutOfMemory, cap::CapError::OutOfMemory(), OUT_OF_MEMORY);
            VERIFY_ERROR(cap::CapError, OperationRejected,
                         cap::CapError::OperationRejected(cap::CapError::Operation::ROOT_DETACH),
                         INVALID_STATE);

            using Paging = memory::PagingError;
            VERIFY_ERROR(Paging, InvalidRoot, Paging::InvalidRoot(PhyAddr(0x1000)),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(Paging, InvalidOwner, Paging::InvalidOwner(0), INVALID_ARGUMENT);
            VERIFY_ERROR(Paging, InvalidState, Paging::InvalidState(Paging::Operation::QUERY),
                         INVALID_STATE);
            VERIFY_ERROR(Paging, IdentifierExhausted,
                         Paging::IdentifierExhausted(Paging::Identifier::ASID), RESOURCE_EXHAUSTED);
            VERIFY_ERROR(Paging, NonCanonicalAddress,
                         Paging::NonCanonicalAddress(Paging::Operation::MAP, 0x1234), OUT_OF_RANGE);
            VERIFY_ERROR(Paging, UnalignedRange,
                         Paging::UnalignedRange(Paging::Operation::MAP, 0x1234, PAGE_SIZE),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(Paging, RangeOverflow,
                         Paging::RangeOverflow(Paging::Operation::MAP, addr_t(-1), PAGE_SIZE),
                         OUT_OF_RANGE);
            VERIFY_ERROR(Paging, OutsideAddressDomain,
                         Paging::OutsideAddressDomain(Paging::Operation::QUERY, KPA_START),
                         OUT_OF_RANGE);
            VERIFY_ERROR(Paging, InvalidPhysicalAddress,
                         Paging::InvalidPhysicalAddress(Paging::Operation::MAP, PhyAddr(0x1000)),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(
                Paging, InvalidFlags,
                Paging::InvalidFlags(memory::PageFlags{.writable = true, .executable = true}),
                INVALID_ARGUMENT);
            VERIFY_ERROR(Paging, MissingMapping, Paging::MissingMapping(0x4000), NOT_FOUND);
            VERIFY_ERROR(Paging, MappingAlreadyPresent, Paging::MappingAlreadyPresent(0x4000),
                         ALREADY_EXISTS);
            VERIFY_ERROR(Paging, UnexpectedEntry, Paging::UnexpectedEntry(0x4000, 2), CONFLICT);
            VERIFY_ERROR(Paging, UnsupportedLeafLevel, Paging::UnsupportedLeafLevel(0x4000, 3),
                         UNSUPPORTED);
            VERIFY_ERROR(Paging, PageTableAllocationFailed,
                         Paging::PageTableAllocationFailed(2, KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Paging, OutOfMemory, Paging::OutOfMemory(), OUT_OF_MEMORY);

            using SegmentError = memory::MemorySegmentError;
            VERIFY_ERROR(SegmentError, ZeroSize, SegmentError::ZeroSize(), INVALID_ARGUMENT);
            VERIFY_ERROR(SegmentError, SizeOverflow, SegmentError::SizeOverflow(size_t(-1)),
                         RESOURCE_EXHAUSTED);
            VERIFY_ERROR(SegmentError, OffsetOutOfRange, SegmentError::OffsetOutOfRange(8, 4),
                         OUT_OF_RANGE);
            VERIFY_ERROR(SegmentError, PageNotAllocated, SegmentError::PageNotAllocated(7),
                         NOT_FOUND);
            VERIFY_ERROR(SegmentError, InvalidSourceBuffer, SegmentError::InvalidSourceBuffer(),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(
                SegmentError, PhysicalAllocationFailed,
                SegmentError::PhysicalAllocationFailed(7, KernelError::TayError::OUT_OF_MEMORY),
                OUT_OF_MEMORY);
            VERIFY_ERROR(SegmentError, PageIndexInsertFailed,
                         SegmentError::PageIndexInsertFailed(7, KernelError::TayError::INTERNAL),
                         CONFLICT);
            VERIFY_ERROR(SegmentError, OutOfMemory, SegmentError::OutOfMemory(), OUT_OF_MEMORY);

            const VirArea user_a{VirAddr(0x1000), VirAddr(0x3000)};
            const VirArea user_b{VirAddr(0x2000), VirAddr(0x4000)};
            using AddressError = task::AddressSpaceError;
            VERIFY_ERROR(AddressError, InvalidSegment, AddressError::InvalidSegment(),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(AddressError, InvalidArea, AddressError::InvalidArea(user_a),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(
                AddressError, InvalidFlags,
                AddressError::InvalidFlags(memory::PageFlags{.writable = true, .executable = true}),
                INVALID_ARGUMENT);
            VERIFY_ERROR(AddressError, SegmentOffsetOutOfRange,
                         AddressError::SegmentOffsetOutOfRange(9, 8), OUT_OF_RANGE);
            VERIFY_ERROR(AddressError, MappingExceedsSegment,
                         AddressError::MappingExceedsSegment(4, 8, 10), OUT_OF_RANGE);
            VERIFY_ERROR(AddressError, AccessDenied,
                         AddressError::AccessDenied(memory::FaultAccess::WRITE,
                                                    memory::PageFlags{.readable = true}),
                         ACCESS_DENIED);
            const AddressError overlap = AddressError::VmaOverlap(user_a, user_b);
            VERIFY_ERROR(AddressError, VmaOverlap, overlap, CONFLICT);
            bool overlap_payload = false;
            overlap.visit([&](const auto &value) noexcept {
                using Value = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, AddressError::VmaOverlap>)
                    overlap_payload = value.requested == user_a && value.existing == user_b;
            });
            kernel::test::require(overlap_payload, "VmaOverlap payload 丢失");
            VERIFY_ERROR(AddressError, VmaNotOwned, AddressError::VmaNotOwned(), NOT_FOUND);
            VERIFY_ERROR(AddressError, UnmappedAddress,
                         AddressError::UnmappedAddress(VirAddr(0x5000)), NOT_FOUND);
            VERIFY_ERROR(AddressError, MappingChanged,
                         AddressError::MappingChanged(VirAddr(0x5000), 3), STALE);
            VERIFY_ERROR(
                AddressError, BackingAllocationFailed,
                AddressError::BackingAllocationFailed(4, KernelError::TayError::OUT_OF_MEMORY),
                OUT_OF_MEMORY);
            VERIFY_ERROR(
                AddressError, PageTableFailed,
                AddressError::PageTableFailed(VirAddr(0x5000), KernelError::TayError::INTERNAL),
                CONFLICT);
            VERIFY_ERROR(AddressError, OutOfMemory, AddressError::OutOfMemory(), OUT_OF_MEMORY);

            using LayoutError = memory::KernelLayoutError;
            const memory::KernelLayoutSpec kernel_layout{
                .virtual_base  = KvaAddr(KVA_START),
                .physical_base = PhyAddr(0x1000),
                .bytes         = PAGE_SIZE,
            };
            const memory::HHDMLayout hhdm_layout{
                .virtual_base  = KpaAddr(KPA_START),
                .physical_base = PhyAddr(0x1000),
                .bytes         = PAGE_SIZE,
            };
            const memory::ReservedLayout reserved_layout{
                .parent        = 2,
                .physical_base = PhyAddr(0x1000),
                .bytes         = PAGE_SIZE,
            };
            const memory::KvaArea kva_a{KvaAddr(KVA_START), KvaAddr(KVA_START + PAGE_SIZE)};
            const memory::KvaArea kva_b{KvaAddr(KVA_START + PAGE_SIZE),
                                        KvaAddr(KVA_START + PAGE_SIZE * 2)};
            const PhyArea phy_a{PhyAddr(0x1000), PhyAddr(0x2000)};
            const PhyArea phy_b{PhyAddr(0x2000), PhyAddr(0x3000)};
            VERIFY_ERROR(LayoutError, InitializationAlreadyAttempted,
                         LayoutError::InitializationAlreadyAttempted(), INVALID_STATE);
            VERIFY_ERROR(LayoutError, DependencyNotReady,
                         LayoutError::DependencyNotReady(LayoutError::Dependency::HEAP),
                         INVALID_STATE);
            VERIFY_ERROR(LayoutError, InvalidKernelLayout,
                         LayoutError::InvalidKernelLayout(kernel_layout), INVALID_ARGUMENT);
            VERIFY_ERROR(LayoutError, InvalidHhdmLayout,
                         LayoutError::InvalidHhdmLayout(hhdm_layout), INVALID_ARGUMENT);
            VERIFY_ERROR(LayoutError, InvalidReservedLayout,
                         LayoutError::InvalidReservedLayout(reserved_layout), INVALID_ARGUMENT);
            VERIFY_ERROR(LayoutError, LayoutConflict, LayoutError::LayoutConflict(kva_a, kva_b),
                         CONFLICT);
            VERIFY_ERROR(LayoutError, HhdmConflict, LayoutError::HhdmConflict(phy_a, phy_b),
                         CONFLICT);
            VERIFY_ERROR(LayoutError, KernelLayoutNotFound, LayoutError::KernelLayoutNotFound(1),
                         NOT_FOUND);
            VERIFY_ERROR(LayoutError, HhdmLayoutNotFound, LayoutError::HhdmLayoutNotFound(2),
                         NOT_FOUND);
            VERIFY_ERROR(LayoutError, ReservedLayoutNotFound,
                         LayoutError::ReservedLayoutNotFound(3), NOT_FOUND);
            VERIFY_ERROR(LayoutError, HhdmCoverageMissing,
                         LayoutError::HhdmCoverageMissing(PhyAddr(0x1000), PAGE_SIZE), NOT_FOUND);
            VERIFY_ERROR(LayoutError, ReservationOwnedByKernel,
                         LayoutError::ReservationOwnedByKernel(3, 1), ACCESS_DENIED);
            VERIFY_ERROR(LayoutError, ReservationsPresent, LayoutError::ReservationsPresent(2),
                         BUSY);
            VERIFY_ERROR(LayoutError, OwnershipMismatch, LayoutError::OwnershipMismatch(1),
                         INTERNAL);
            VERIFY_ERROR(LayoutError, NodeAllocationFailed,
                         LayoutError::NodeAllocationFailed(LayoutError::LayoutKind::KERNEL, true),
                         RESOURCE_EXHAUSTED);
            VERIFY_ERROR(LayoutError, PagingFailed,
                         LayoutError::PagingFailed(Paging::MissingMapping(KVA_START)), NOT_FOUND);
        }

        void verify_task_runtime_errors() noexcept {
            using Process = task::ProcessError;
            VERIFY_ERROR(Process, OutOfMemory, Process::OutOfMemory(), OUT_OF_MEMORY);
            VERIFY_ERROR(Process, InvalidState, Process::InvalidState(task::ProcessState::DEAD),
                         INVALID_STATE);
            VERIFY_ERROR(Process, KernelProcessOperation, Process::KernelProcessOperation(),
                         ACCESS_DENIED);
            VERIFY_ERROR(Process, AddressSpaceAlreadySet, Process::AddressSpaceAlreadySet(),
                         ALREADY_EXISTS);
            VERIFY_ERROR(Process, CSpaceAlreadySet, Process::CSpaceAlreadySet(), ALREADY_EXISTS);
            VERIFY_ERROR(Process, MissingAddressSpace, Process::MissingAddressSpace(), NOT_FOUND);
            VERIFY_ERROR(Process, MissingCSpace, Process::MissingCSpace(), NOT_FOUND);
            VERIFY_ERROR(Process, AlreadySubmitted, Process::AlreadySubmitted(), ALREADY_EXISTS);

            using Thread = task::ThreadError;
            VERIFY_ERROR(Thread, InvalidStackSize, Thread::InvalidStackSize(1), INVALID_ARGUMENT);
            VERIFY_ERROR(Thread, HeapUnavailable, Thread::HeapUnavailable(), INVALID_STATE);
            VERIFY_ERROR(Thread, InvalidEntry, Thread::InvalidEntry(0), INVALID_ARGUMENT);
            VERIFY_ERROR(Thread, InvalidUserStack, Thread::InvalidUserStack(3), INVALID_ARGUMENT);
            VERIFY_ERROR(Thread, InvalidMode, Thread::InvalidMode(task::ThreadMode::KERNEL),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(Thread, InvalidProcessState,
                         Thread::InvalidProcessState(task::ProcessState::DEAD), INVALID_STATE);
            VERIFY_ERROR(Thread, AlreadyConfigured, Thread::AlreadyConfigured(), ALREADY_EXISTS);
            VERIFY_ERROR(Thread, SchedulerAttached, Thread::SchedulerAttached(), BUSY);
            VERIFY_ERROR(Thread, NotCurrentThread, Thread::NotCurrentThread(), INVALID_STATE);
            VERIFY_ERROR(Thread, InvalidThreadState,
                         Thread::InvalidThreadState(task::ThreadState::EXITED), INVALID_STATE);
            VERIFY_ERROR(Thread, TimedWaitActive, Thread::TimedWaitActive(), BUSY);
            VERIFY_ERROR(Thread, PinFailed, Thread::PinFailed(), BUSY);
            VERIFY_ERROR(Thread, TimerArmFailed,
                         Thread::TimerArmFailed(KernelError::TayError::INTERNAL), INVALID_STATE);
            VERIFY_ERROR(Thread, OutOfMemory, Thread::OutOfMemory(), OUT_OF_MEMORY);

            using Scheduler = scheduler::SchedulerError;
            VERIFY_ERROR(Scheduler, NotReady, Scheduler::NotReady(), INVALID_STATE);
            VERIFY_ERROR(Scheduler, AlreadyReady, Scheduler::AlreadyReady(), ALREADY_EXISTS);
            VERIFY_ERROR(Scheduler, BootstrapNotRunning,
                         Scheduler::BootstrapNotRunning(task::ThreadState::CREATED), INVALID_STATE);
            VERIFY_ERROR(Scheduler, InterruptsEnabled, Scheduler::InterruptsEnabled(),
                         INVALID_STATE);
            VERIFY_ERROR(Scheduler, ThreadAlreadyAttached, Scheduler::ThreadAlreadyAttached(),
                         ALREADY_EXISTS);
            VERIFY_ERROR(Scheduler, ThreadNotAttached, Scheduler::ThreadNotAttached(), NOT_FOUND);
            VERIFY_ERROR(Scheduler, ThreadNotConfigured, Scheduler::ThreadNotConfigured(),
                         INVALID_STATE);
            VERIFY_ERROR(Scheduler, ThreadNotSubmitted, Scheduler::ThreadNotSubmitted(),
                         INVALID_STATE);
            VERIFY_ERROR(Scheduler, InvalidThreadState,
                         Scheduler::InvalidThreadState(task::ThreadState::EXITED), INVALID_STATE);
            VERIFY_ERROR(Scheduler, QueueStateMismatch,
                         Scheduler::QueueStateMismatch(scheduler::QueueState::MIGRATING), CONFLICT);
            VERIFY_ERROR(Scheduler, TimedWaitActive, Scheduler::TimedWaitActive(), BUSY);
            VERIFY_ERROR(Scheduler, InvalidBlockToken, Scheduler::InvalidBlockToken(),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(Scheduler, NoRunnableThread, Scheduler::NoRunnableThread(), NOT_FOUND);
            VERIFY_ERROR(Scheduler, IdleThreadOperation, Scheduler::IdleThreadOperation(),
                         ACCESS_DENIED);
            VERIFY_ERROR(Scheduler, DeadlineSinkAlreadyInstalled,
                         Scheduler::DeadlineSinkAlreadyInstalled(), ALREADY_EXISTS);
            VERIFY_ERROR(Scheduler, InvalidDeadlineSink, Scheduler::InvalidDeadlineSink(),
                         INVALID_ARGUMENT);

            using Timer = kernel::timer::TimerError;
            VERIFY_ERROR(Timer, WorkQueueNotAccepting, Timer::WorkQueueNotAccepting(), BUSY);
            VERIFY_ERROR(Timer, EngineNotInitialized, Timer::EngineNotInitialized(), INVALID_STATE);
            VERIFY_ERROR(Timer, NodeNotIdle,
                         Timer::NodeNotIdle(kernel::timer::PrecisionTimerState::QUEUED), BUSY);
            VERIFY_ERROR(Timer, NodeAlreadyLinked, Timer::NodeAlreadyLinked(), CONFLICT);
            VERIFY_ERROR(Timer, CompletionNotReservable, Timer::CompletionNotReservable(), BUSY);

            using Queue = kernel::async::WorkQueueError;
            VERIFY_ERROR(Queue, AlreadyStarted, Queue::AlreadyStarted(), ALREADY_EXISTS);
            VERIFY_ERROR(Queue, PendingWork, Queue::PendingWork(), BUSY);
            VERIFY_ERROR(Queue, WorkerCreationFailed,
                         Queue::WorkerCreationFailed(KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Queue, WorkerAttachFailed,
                         Queue::WorkerAttachFailed(KernelError::TayError::INTERNAL), INVALID_STATE);
            VERIFY_ERROR(Queue, NotStarted, Queue::NotStarted(), INVALID_STATE);
            VERIFY_ERROR(Queue, PermanentQueue, Queue::PermanentQueue(), ACCESS_DENIED);
            VERIFY_ERROR(Queue, CalledByWorker, Queue::CalledByWorker(), BUSY);
        }

        void verify_usrboot_errors() noexcept {
            using Error = init::UsrbootError;
            VERIFY_ERROR(Error, ImageTooSmall, Error::ImageTooSmall(4, 120), MALFORMED_INPUT);
            VERIFY_ERROR(Error, InvalidMagic, Error::InvalidMagic(0), MALFORMED_INPUT);
            VERIFY_ERROR(Error, InvalidHeader, Error::InvalidHeader(120, 1, 0), MALFORMED_INPUT);
            VERIFY_ERROR(Error, InvalidSegmentSize,
                         Error::InvalidSegmentSize(Error::Segment::RX, 1, 2), MALFORMED_INPUT);
            VERIFY_ERROR(Error, SegmentAddressOverflow,
                         Error::SegmentAddressOverflow(Error::Segment::RW, addr_t(-1), 2),
                         OUT_OF_RANGE);
            VERIFY_ERROR(
                Error, SegmentOutsideUserRange,
                Error::SegmentOutsideUserRange(Error::Segment::RO, KPA_START, KPA_START + 1),
                OUT_OF_RANGE);
            VERIFY_ERROR(Error, SegmentFileRangeInvalid,
                         Error::SegmentFileRangeInvalid(Error::Segment::RX, 200, 8, 120),
                         MALFORMED_INPUT);
            VERIFY_ERROR(Error, SegmentUnaligned, Error::SegmentUnaligned(Error::Segment::RW, 3),
                         MALFORMED_INPUT);
            VERIFY_ERROR(Error, ObjectCreationFailed,
                         Error::ObjectCreationFailed(Error::Object::PROCESS,
                                                     KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Error, ProcessConfigurationFailed,
                         Error::ProcessConfigurationFailed(KernelError::TayError::INTERNAL),
                         INVALID_STATE);
            VERIFY_ERROR(
                Error, VmaCreationFailed,
                Error::VmaCreationFailed(Error::Segment::RO, KernelError::TayError::INTERNAL),
                CONFLICT);
            VERIFY_ERROR(Error, SegmentWriteFailed,
                         Error::SegmentWriteFailed(Error::Segment::RW, 7,
                                                   KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Error, InitialStackFailed,
                         Error::InitialStackFailed(KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Error, ThreadCreationFailed,
                         Error::ThreadCreationFailed(KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(
                Error, UserContextConfigurationFailed,
                Error::UserContextConfigurationFailed(KernelError::TayError::INVALID_ARGUMENT),
                INVALID_ARGUMENT);
            VERIFY_ERROR(Error, ProcessSubmissionFailed,
                         Error::ProcessSubmissionFailed(KernelError::TayError::INTERNAL),
                         INVALID_STATE);
            VERIFY_ERROR(Error, ThreadAttachFailed,
                         Error::ThreadAttachFailed(KernelError::TayError::INTERNAL), INVALID_STATE);
        }

        void verify_device_errors() noexcept {
            const device::FirmwareId id{
                .kind = device::FirmwareKind::FDT, .namespace_id = 1, .local_id = 2};
            const device::FirmwareId parent{
                .kind = device::FirmwareKind::FDT, .namespace_id = 1, .local_id = 1};
            const PhyArea phy_a{PhyAddr(0x1000), PhyAddr(0x2000)};
            const PhyArea phy_b{PhyAddr(0x1800), PhyAddr(0x2800)};

            using Catalog = device::CatalogError;
            VERIFY_ERROR(Catalog, InvalidDescriptor,
                         Catalog::InvalidDescriptor(device::FirmwareId{}), INVALID_ARGUMENT);
            VERIFY_ERROR(Catalog, DuplicateFirmwareId, Catalog::DuplicateFirmwareId(id),
                         ALREADY_EXISTS);
            VERIFY_ERROR(Catalog, DuplicateLogicalCpu, Catalog::DuplicateLogicalCpu(2),
                         ALREADY_EXISTS);
            VERIFY_ERROR(Catalog, ParentNotFound, Catalog::ParentNotFound(id, parent), NOT_FOUND);
            VERIFY_ERROR(Catalog, ResourceOverlap, Catalog::ResourceOverlap(id, phy_a, phy_b),
                         CONFLICT);
            VERIFY_ERROR(Catalog, InterruptControllerNotFound,
                         Catalog::InterruptControllerNotFound(id, parent), NOT_FOUND);
            VERIFY_ERROR(Catalog, CapacityExhausted,
                         Catalog::CapacityExhausted(Catalog::EntryKind::DEVICE),
                         RESOURCE_EXHAUSTED);
            VERIFY_ERROR(Catalog, NoCpuDiscovered, Catalog::NoCpuDiscovered(), NOT_FOUND);
            VERIFY_ERROR(Catalog, BackendFailed,
                         Catalog::BackendFailed(KernelError::TayError::INTERNAL), MALFORMED_INPUT);
            VERIFY_ERROR(Catalog, OutOfMemory, Catalog::OutOfMemory(), OUT_OF_MEMORY);

            using Fdt = device::FdtError;
            VERIFY_ERROR(Fdt, InvalidBlob, Fdt::InvalidBlob(), MALFORMED_INPUT);
            VERIFY_ERROR(Fdt, NodeNotFound, Fdt::NodeNotFound(3), NOT_FOUND);
            VERIFY_ERROR(Fdt, MissingProperty, Fdt::MissingProperty(3, device::PropertyId::REG),
                         NOT_FOUND);
            VERIFY_ERROR(Fdt, InvalidProperty, Fdt::InvalidProperty(3, device::PropertyId::REG),
                         MALFORMED_INPUT);
            VERIFY_ERROR(Fdt, CellCountUnsupported, Fdt::CellCountUnsupported(3, 4, 2),
                         UNSUPPORTED);
            VERIFY_ERROR(Fdt, AddressTranslationFailed, Fdt::AddressTranslationFailed(3),
                         OUT_OF_RANGE);
            VERIFY_ERROR(Fdt, IntegerOverflow, Fdt::IntegerOverflow(3, device::PropertyId::REG),
                         OUT_OF_RANGE);
            VERIFY_ERROR(Fdt, BootCpuNotFound, Fdt::BootCpuNotFound(7), NOT_FOUND);
            VERIFY_ERROR(Fdt, CatalogRejected,
                         Fdt::CatalogRejected(KernelError::TayError::INTERNAL), RESOURCE_EXHAUSTED);

            using Mmio = device::MmioError;
            VERIFY_ERROR(Mmio, InvalidPhysicalArea, Mmio::InvalidPhysicalArea(phy_a),
                         INVALID_ARGUMENT);
            VERIFY_ERROR(Mmio, SizeOverflow, Mmio::SizeOverflow(PhyAddr(0x1000), size_t(-1)),
                         OUT_OF_RANGE);
            VERIFY_ERROR(Mmio, MappingConflict, Mmio::MappingConflict(phy_a), CONFLICT);
            VERIFY_ERROR(Mmio, KernelSpaceUnavailable, Mmio::KernelSpaceUnavailable(),
                         INVALID_STATE);
            VERIFY_ERROR(Mmio, NotMapped, Mmio::NotMapped(), NOT_FOUND);
            VERIFY_ERROR(Mmio, PagingFailed,
                         Mmio::PagingFailed(memory::PagingError::MissingMapping(KVA_START)),
                         NOT_FOUND);
            VERIFY_ERROR(Mmio, OutOfMemory, Mmio::OutOfMemory(), OUT_OF_MEMORY);

#if defined(__ARCH_RISCV64__)
            using Plic = riscv64::device::interrupt::PlicError;
            VERIFY_ERROR(Plic, InvalidMmioRange, Plic::InvalidMmioRange(phy_a), INVALID_ARGUMENT);
            VERIFY_ERROR(Plic, ContextOutOfRange, Plic::ContextOutOfRange(4, 2), OUT_OF_RANGE);
            const Plic source = Plic::SourceOutOfRange(11, 10);
            VERIFY_ERROR(Plic, SourceOutOfRange, source, OUT_OF_RANGE);
            bool source_payload = false;
            source.visit([&](const auto &value) noexcept {
                using Value = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Plic::SourceOutOfRange>)
                    source_payload = value.source == 11 && value.source_count == 10;
            });
            kernel::test::require(source_payload, "Plic SourceOutOfRange payload 丢失");
            VERIFY_ERROR(Plic, InvalidPriority, Plic::InvalidPriority(8, 7), INVALID_ARGUMENT);
            VERIFY_ERROR(Plic, MissingMmio, Plic::MissingMmio(), NOT_FOUND);
            VERIFY_ERROR(Plic, MissingContext, Plic::MissingContext(), NOT_FOUND);
            VERIFY_ERROR(Plic, InvalidClaim, Plic::InvalidClaim(), INVALID_ARGUMENT);
            VERIFY_ERROR(Plic, ControllerNotFound, Plic::ControllerNotFound(), NOT_FOUND);
            VERIFY_ERROR(Plic, MmioFailed, Plic::MmioFailed(KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Plic, PlicAllocationFailed,
                         Plic::PlicAllocationFailed(KernelError::TayError::OUT_OF_MEMORY),
                         OUT_OF_MEMORY);
            VERIFY_ERROR(Plic, DomainRegistrationFailed,
                         Plic::DomainRegistrationFailed(KernelError::TayError::INTERNAL), CONFLICT);
#endif
        }
    }  // namespace

    void run_error_model(Context &) noexcept {
        kernel::test::require(!from_tay_error(tay::error_code::NONE),
                              "tay::error_code::NONE 被错误归类为失败");
        kernel::test::require(
            maps_to(tay::error_code::OUT_OF_MEMORY, KernelError::TayError::OUT_OF_MEMORY),
            "tay::error_code::OUT_OF_MEMORY 归类错误");
        verify_p0_errors();
        verify_task_runtime_errors();
        verify_usrboot_errors();
        verify_device_errors();

        tay::expected<void, scheduler::SchedulerError> direct_alternative =
            tay::Err(scheduler::SchedulerError::AlreadyReady());
        kernel::test::require(
            !direct_alternative &&
                direct_alternative.error().is<scheduler::SchedulerError::AlreadyReady>(),
            "Error::Alternative() 未自动提升为领域错误 wrapper");

        tay::expected<u32_t, memory::PagingError> success{7};
        kernel::test::require(success && *success == 7, "领域 expected 成功值错误");
        tay::expected<u32_t, memory::PagingError> failure =
            tay::Err(memory::PagingError::MissingMapping(0x7000));
        auto reduced = failure.transform_error(
            [](const memory::PagingError &error) noexcept { return error.code(); });
        kernel::test::require(
            !reduced && reduced.error() == KernelError::PagingError::MISSING_MAPPING,
            "领域 expected transform_error() 归约错误");
    }

#undef VERIFY_ERROR
}  // namespace kernel::test::cases
