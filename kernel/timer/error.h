/**
 * @file error.h
 * @brief 定义 PrecisionTimerEngine::arm 的可恢复状态错误。
 */

#pragma once

#include <error.h>
#include <tay/utility.h>
#include <tay/variant.h>
#include <timer/timer_node.h>

#include <type_traits>
#include <utility>

namespace kernel::timer {
    class TimerError final {
    public:
        using kernel_domain_error_tag = void;

        struct WorkQueueNotAccepting {};
        struct EngineNotInitialized {};
        struct NodeNotIdle {
            PrecisionTimerState state = PrecisionTimerState::IDLE;
        };
        struct NodeAlreadyLinked {};
        struct CompletionNotReservable {};

        TimerError()                                  = delete;
        TimerError(const TimerError &)                = default;
        TimerError &operator=(const TimerError &)     = default;
        TimerError(TimerError &&) noexcept            = default;
        TimerError &operator=(TimerError &&) noexcept = default;
        ~TimerError() noexcept                        = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::TimerError;
            return visit(tay::overloaded{
                [](const WorkQueueNotAccepting &) noexcept {
                    return Reason::WORK_QUEUE_NOT_ACCEPTING;
                },
                [](const EngineNotInitialized &) noexcept {
                    return Reason::ENGINE_NOT_INITIALIZED;
                },
                [](const NodeNotIdle &) noexcept { return Reason::NODE_NOT_IDLE; },
                [](const NodeAlreadyLinked &) noexcept { return Reason::NODE_ALREADY_LINKED; },
                [](const CompletionNotReservable &) noexcept {
                    return Reason::COMPLETION_NOT_RESERVABLE;
                },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const WorkQueueNotAccepting &) noexcept {
                    return "timer completion work queue is not accepting";
                },
                [](const EngineNotInitialized &) noexcept {
                    return "precision timer engine is not initialized";
                },
                [](const NodeNotIdle &) noexcept { return "precision timer node is not idle"; },
                [](const NodeAlreadyLinked &) noexcept {
                    return "precision timer node is already linked";
                },
                [](const CompletionNotReservable &) noexcept {
                    return "timer completion cannot be reserved";
                },
            });
        }

    private:
        using Storage = tay::variant<WorkQueueNotAccepting, EngineNotInitialized, NodeNotIdle,
                                     NodeAlreadyLinked, CompletionNotReservable>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        TimerError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(TimerError) <= 16);
    static_assert(std::is_nothrow_move_constructible_v<TimerError>);
}  // namespace kernel::timer
