/**
 * @file sched.h
 * @brief 定义 SchedCore 可恢复的生命周期与状态转换错误。
 */

#pragma once

#include <error.h>
#include <scheduler/entity.h>
#include <task/state.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace scheduler {
    class SchedulerError final {
    public:
        using kernel_domain_error_tag = void;

        struct NotReady {};
        struct AlreadyReady {};
        struct BootstrapNotRunning {
            task::ThreadState state = task::ThreadState::CREATED;
        };
        struct InterruptsEnabled {};
        struct ThreadAlreadyAttached {};
        struct ThreadNotAttached {};
        struct ThreadNotConfigured {};
        struct ThreadNotSubmitted {};
        struct InvalidThreadState {
            task::ThreadState state = task::ThreadState::CREATED;
        };
        struct QueueStateMismatch {
            QueueState state = QueueState::DETACHED;
        };
        struct TimedWaitActive {};
        struct InvalidBlockToken {};
        struct NoRunnableThread {};
        struct IdleThreadOperation {};
        struct PreemptSinkAlreadySet {};
        struct InvalidDeadlineSink {};

        SchedulerError()                                      = delete;
        SchedulerError(const SchedulerError &)                = default;
        SchedulerError &operator=(const SchedulerError &)     = default;
        SchedulerError(SchedulerError &&) noexcept            = default;
        SchedulerError &operator=(SchedulerError &&) noexcept = default;
        ~SchedulerError() noexcept                            = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::SchedulerError;
            return visit(tay::overloaded{
                [](const NotReady &) noexcept { return Reason::NOT_READY; },
                [](const AlreadyReady &) noexcept { return Reason::ALREADY_READY; },
                [](const BootstrapNotRunning &) noexcept { return Reason::BOOTSTRAP_NOT_RUNNING; },
                [](const InterruptsEnabled &) noexcept { return Reason::INTERRUPTS_ENABLED; },
                [](const ThreadAlreadyAttached &) noexcept {
                    return Reason::THREAD_ALREADY_ATTACHED;
                },
                [](const ThreadNotAttached &) noexcept { return Reason::THREAD_NOT_ATTACHED; },
                [](const ThreadNotConfigured &) noexcept { return Reason::THREAD_NOT_CONFIGURED; },
                [](const ThreadNotSubmitted &) noexcept { return Reason::THREAD_NOT_SUBMITTED; },
                [](const InvalidThreadState &) noexcept { return Reason::INVALID_THREAD_STATE; },
                [](const QueueStateMismatch &) noexcept { return Reason::QUEUE_STATE_MISMATCH; },
                [](const TimedWaitActive &) noexcept { return Reason::TIMED_WAIT_ACTIVE; },
                [](const InvalidBlockToken &) noexcept { return Reason::INVALID_BLOCK_TOKEN; },
                [](const NoRunnableThread &) noexcept { return Reason::NO_RUNNABLE_THREAD; },
                [](const IdleThreadOperation &) noexcept { return Reason::IDLE_THREAD_OPERATION; },
                [](const PreemptSinkAlreadySet &) noexcept {
                    return Reason::PREEMPT_SINK_ALREADY_SET;
                },
                [](const InvalidDeadlineSink &) noexcept { return Reason::INVALID_DEADLINE_SINK; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const NotReady &) noexcept { return "scheduler is not ready"; },
                [](const AlreadyReady &) noexcept { return "scheduler is already ready"; },
                [](const BootstrapNotRunning &) noexcept {
                    return "scheduler bootstrap thread is not running";
                },
                [](const InterruptsEnabled &) noexcept {
                    return "scheduler operation requires local interrupts disabled";
                },
                [](const ThreadAlreadyAttached &) noexcept {
                    return "thread is already attached to scheduler";
                },
                [](const ThreadNotAttached &) noexcept {
                    return "thread is not attached to scheduler";
                },
                [](const ThreadNotConfigured &) noexcept { return "thread is not configured"; },
                [](const ThreadNotSubmitted &) noexcept {
                    return "thread process is not submitted";
                },
                [](const InvalidThreadState &) noexcept {
                    return "thread state rejects scheduler operation";
                },
                [](const QueueStateMismatch &) noexcept {
                    return "thread run-queue state is inconsistent";
                },
                [](const TimedWaitActive &) noexcept { return "thread has an active timed wait"; },
                [](const InvalidBlockToken &) noexcept {
                    return "scheduler block token is invalid";
                },
                [](const NoRunnableThread &) noexcept {
                    return "scheduler has no runnable replacement thread";
                },
                [](const IdleThreadOperation &) noexcept {
                    return "operation is not allowed on idle thread";
                },
                [](const PreemptSinkAlreadySet &) noexcept {
                    return "scheduler deadline sink is already installed";
                },
                [](const InvalidDeadlineSink &) noexcept {
                    return "scheduler deadline sink is invalid";
                },
            });
        }

    private:
        using Storage =
            tay::variant<NotReady, AlreadyReady, BootstrapNotRunning, InterruptsEnabled,
                         ThreadAlreadyAttached, ThreadNotAttached, ThreadNotConfigured,
                         ThreadNotSubmitted, InvalidThreadState, QueueStateMismatch,
                         TimedWaitActive, InvalidBlockToken, NoRunnableThread, IdleThreadOperation,
                         PreemptSinkAlreadySet, InvalidDeadlineSink>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        SchedulerError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(SchedulerError) <= 16);
    static_assert(std::is_nothrow_move_constructible_v<SchedulerError>);
}  // namespace scheduler
