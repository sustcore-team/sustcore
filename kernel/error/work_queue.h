/**
 * @file work_queue.h
 * @brief 定义 WorkQueue 生命周期转换错误。
 */

#pragma once

#include <error.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace kernel::async {
    class WorkQueueError final {
    public:
        using kernel_domain_error_tag = void;

        struct AlreadyStarted {};
        struct PendingWork {};
        struct WorkerCreateFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct WorkerAttachFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct NotStarted {};
        struct PermanentQueue {};
        struct CalledByWorker {};

        WorkQueueError()                                      = delete;
        WorkQueueError(const WorkQueueError &)                = default;
        WorkQueueError &operator=(const WorkQueueError &)     = default;
        WorkQueueError(WorkQueueError &&) noexcept            = default;
        WorkQueueError &operator=(WorkQueueError &&) noexcept = default;
        ~WorkQueueError() noexcept                            = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::WorkQueueError;
            return visit(tay::overloaded{
                [](const AlreadyStarted &) noexcept { return Reason::ALREADY_STARTED; },
                [](const PendingWork &) noexcept { return Reason::PENDING_WORK; },
                [](const WorkerCreateFailed &) noexcept { return Reason::WORKER_CREATE_FAILED; },
                [](const WorkerAttachFailed &) noexcept { return Reason::WORKER_ATTACH_FAILED; },
                [](const NotStarted &) noexcept { return Reason::NOT_STARTED; },
                [](const PermanentQueue &) noexcept { return Reason::PERMANENT_QUEUE; },
                [](const CalledByWorker &) noexcept { return Reason::CALLED_BY_WORKER; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const AlreadyStarted &) noexcept { return "work queue is already started"; },
                [](const PendingWork &) noexcept { return "work queue contains pending work"; },
                [](const WorkerCreateFailed &) noexcept {
                    return "work queue worker creation failed";
                },
                [](const WorkerAttachFailed &) noexcept {
                    return "work queue worker scheduler attach failed";
                },
                [](const NotStarted &) noexcept { return "work queue is not started"; },
                [](const PermanentQueue &) noexcept { return "permanent work queue cannot stop"; },
                [](const CalledByWorker &) noexcept {
                    return "work queue worker cannot shut down its own queue";
                },
            });
        }

    private:
        using Storage =
            tay::variant<AlreadyStarted, PendingWork, WorkerCreateFailed, WorkerAttachFailed,
                         NotStarted, PermanentQueue, CalledByWorker>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        WorkQueueError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(WorkQueueError) <= 16);
    static_assert(std::is_nothrow_move_constructible_v<WorkQueueError>);
}  // namespace kernel::async
