/**
 * @file thread_error.h
 * @brief 定义 Thread 创建、栈和用户上下文配置错误。
 */

#pragma once

#include <error.h>
#include <task/state.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace task {
    class ThreadError final {
    public:
        using kernel_domain_error_tag = void;

        struct InvalidStackSize {
            size_t bytes = 0;
        };
        struct HeapUnavailable {};
        struct InvalidEntry {
            addr_t entry = 0;
        };
        struct InvalidUserStack {
            addr_t stack_pointer = 0;
        };
        struct InvalidMode {
            ThreadMode mode = ThreadMode::KERNEL;
        };
        struct InvalidProcessState {
            ProcessState state = ProcessState::CREATED;
        };
        struct AlreadyConfigured {};
        struct SchedulerAttached {};
        struct NotCurrentThread {};
        struct InvalidThreadState {
            ThreadState state = ThreadState::CREATED;
        };
        struct TimedWaitActive {};
        struct PinFailed {};
        struct TimerArmFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct OutOfMemory {};

        ThreadError()                                   = delete;
        ThreadError(const ThreadError &)                = default;
        ThreadError &operator=(const ThreadError &)     = default;
        ThreadError(ThreadError &&) noexcept            = default;
        ThreadError &operator=(ThreadError &&) noexcept = default;
        ~ThreadError() noexcept                         = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::ThreadError;
            return visit(tay::overloaded{
                [](const InvalidStackSize &) noexcept { return Reason::INVALID_STACK_SIZE; },
                [](const HeapUnavailable &) noexcept { return Reason::HEAP_UNAVAILABLE; },
                [](const InvalidEntry &) noexcept { return Reason::INVALID_ENTRY; },
                [](const InvalidUserStack &) noexcept { return Reason::INVALID_USER_STACK; },
                [](const InvalidMode &) noexcept { return Reason::INVALID_MODE; },
                [](const InvalidProcessState &) noexcept { return Reason::INVALID_PROCESS_STATE; },
                [](const AlreadyConfigured &) noexcept { return Reason::ALREADY_CONFIGURED; },
                [](const SchedulerAttached &) noexcept { return Reason::SCHEDULER_ATTACHED; },
                [](const NotCurrentThread &) noexcept { return Reason::NOT_CURRENT_THREAD; },
                [](const InvalidThreadState &) noexcept { return Reason::INVALID_THREAD_STATE; },
                [](const TimedWaitActive &) noexcept { return Reason::TIMED_WAIT_ACTIVE; },
                [](const PinFailed &) noexcept { return Reason::PIN_FAILED; },
                [](const TimerArmFailed &) noexcept { return Reason::TIMER_ARM_FAILED; },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidStackSize &) noexcept { return "invalid kernel stack size"; },
                [](const HeapUnavailable &) noexcept { return "kernel heap is unavailable"; },
                [](const InvalidEntry &) noexcept { return "invalid thread entry point"; },
                [](const InvalidUserStack &) noexcept { return "invalid user stack pointer"; },
                [](const InvalidMode &) noexcept { return "thread mode rejects the operation"; },
                [](const InvalidProcessState &) noexcept {
                    return "process state rejects thread creation";
                },
                [](const AlreadyConfigured &) noexcept { return "thread is already configured"; },
                [](const SchedulerAttached &) noexcept {
                    return "thread is already attached to a scheduler";
                },
                [](const NotCurrentThread &) noexcept { return "thread is not current"; },
                [](const InvalidThreadState &) noexcept {
                    return "thread state rejects the operation";
                },
                [](const TimedWaitActive &) noexcept { return "thread already has a timed wait"; },
                [](const PinFailed &) noexcept { return "thread cannot be pinned"; },
                [](const TimerArmFailed &) noexcept {
                    return "thread timed-wait timer arm failed";
                },
                [](const OutOfMemory &) noexcept { return "thread allocation failed"; },
            });
        }

    private:
        using Storage =
            tay::variant<InvalidStackSize, HeapUnavailable, InvalidEntry, InvalidUserStack,
                         InvalidMode, InvalidProcessState, AlreadyConfigured, SchedulerAttached,
                         NotCurrentThread, InvalidThreadState, TimedWaitActive, PinFailed,
                         TimerArmFailed, OutOfMemory>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        ThreadError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(ThreadError) <= 16);
    static_assert(std::is_nothrow_move_constructible_v<ThreadError>);
}  // namespace task
