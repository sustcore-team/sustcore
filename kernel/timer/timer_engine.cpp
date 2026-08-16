/**
 * @file timer_engine.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief precision timer arm/cancel/expire/post exactly-once 状态机。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <async/work_queue.h>
#include <async/worklet.h>
#include <log.h>
#include <tay/lock.h>
#include <timer/timer_engine.h>

namespace kernel::timer {
    namespace {
        constinit PrecisionTimerEngine bsp_engine;
    }  // namespace

    PrecisionTimerEngine &bsp_timer_engine() noexcept {
        return bsp_engine;
    }

    void PrecisionTimerEngine::initialize(DeadlineState &deadlines) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (deadlines_ != nullptr || !heap_.empty())
            kernel::log::panic("invalid PrecisionTimerEngine initialization");
        deadlines_ = &deadlines;
    }

    hal::CpuClockDeadline PrecisionTimerEngine::root_deadline_locked() const noexcept {
        const auto *root = heap_.top();
        return root == nullptr
                   ? hal::CpuClockDeadline::disarmed()
                   : hal::CpuClockDeadline::at(units::time::from_nanoseconds(root->deadline_));
    }

    tay::expected<void, TimerError> PrecisionTimerEngine::arm(PrecisionTimerNode &node,
                                                              units::time deadline,
                                                              async::Worklet &completion) noexcept {
        hal::interrupt_guard interrupt_guard;
        auto &queue = async::bsp_work_queue();
        if (!queue.accepting())
            return tay::Err(TimerError::WorkQueueNotAccepting());
        tay::lock_guard guard(lock_);
        // setup() 必须先于 arm；IDLE -> RESERVED 的 release 发布 typed completion 字段，
        // WorkQueue 在 RESERVED -> QUEUED -> IDLE 的 acquire 链上观察这些字段。
        if (deadlines_ == nullptr)
            return tay::Err(TimerError::EngineNotInitialized());
        if (node.state_ != PrecisionTimerState::IDLE)
            return tay::Err(TimerError::NodeNotIdle(node.state_));
        if (node.heap_hook_.linked || node.due_hook_.in_list)
            return tay::Err(TimerError::NodeAlreadyLinked());
        if (!completion.try_reserve_for_timer(queue))
            return tay::Err(TimerError::CompletionNotReservable());
        node.deadline_   = deadline.to_nanoseconds();
        node.completion_ = &completion;
        node.engine_     = this;
        heap_.push(node);
        node.state_ = PrecisionTimerState::QUEUED;
        // engine lock -> DeadlineState lock 是 timer root publication 的固定锁序。
        deadlines_->publish_timer(root_deadline_locked());
        return {};
    }

    TimerCancelResult PrecisionTimerEngine::cancel(PrecisionTimerNode &node) noexcept {
        hal::interrupt_guard interrupt_guard;
        {
            tay::lock_guard guard(lock_);
            if (node.engine_ != this || node.state_ == PrecisionTimerState::IDLE ||
                node.state_ == PrecisionTimerState::RETIRED)
                return TimerCancelResult::NOT_ARMED;
            if (node.state_ != PrecisionTimerState::QUEUED)
                return TimerCancelResult::RACE_LOST;
            (void)heap_.remove(node);
            node.state_ = PrecisionTimerState::ELAPSED;
            deadlines_->publish_timer(root_deadline_locked());
        }
        post_elapsed(node);
        return TimerCancelResult::CANCELLED;
    }

    void PrecisionTimerEngine::progress(units::time now) noexcept {
        timer_due_list due;
        const auto now_nanoseconds = now.to_nanoseconds();
        hal::interrupt_guard interrupt_guard;
        {
            tay::lock_guard guard(lock_);
            if (deadlines_ == nullptr)
                kernel::log::panic("progressing an uninitialized PrecisionTimerEngine");
            while (heap_.top() != nullptr && heap_.top()->deadline_ <= now_nanoseconds) {
                auto *node = heap_.pop_min();
                if (node->state_ != PrecisionTimerState::QUEUED)
                    kernel::log::panic("timer heap contained a non-queued node");
                node->state_ = PrecisionTimerState::ELAPSED;
                due.push_back(node);
            }
            deadlines_->publish_timer(root_deadline_locked());
        }

        while (!due.empty()) {
            auto *node = due.pop_front();
            post_elapsed(*node);
        }
    }

    void PrecisionTimerEngine::post_elapsed(PrecisionTimerNode &node) noexcept {
        async::WorkQueue *queue    = nullptr;
        async::Worklet *completion = nullptr;
        {
            hal::interrupt_guard interrupt_guard;
            tay::lock_guard guard(lock_);
            if (node.engine_ != this || node.state_ != PrecisionTimerState::ELAPSED ||
                node.due_hook_.in_list || node.completion_ == nullptr)
                kernel::log::panic("posting an inconsistent elapsed timer node");
            node.state_ = PrecisionTimerState::POSTED;
            queue       = &async::bsp_work_queue();
            completion  = node.completion_;
        }
        if (!queue->try_post_reserved(*completion))
            kernel::log::panic("fixed BSP WorkQueue rejected a timer completion");
    }

    void PrecisionTimerEngine::retire(PrecisionTimerNode &node) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (node.engine_ != this || node.state_ != PrecisionTimerState::POSTED ||
            node.heap_hook_.linked || node.due_hook_.in_list)
            kernel::log::panic("retiring an inconsistent posted timer node");
        node.state_ = PrecisionTimerState::RETIRED;
    }

    void PrecisionTimerEngine::reset(PrecisionTimerNode &node) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (node.engine_ != this || node.state_ != PrecisionTimerState::RETIRED ||
            node.heap_hook_.linked || node.due_hook_.in_list)
            kernel::log::panic("resetting a timer node before retirement");
        node.deadline_   = 0;
        node.completion_ = nullptr;
        node.engine_     = nullptr;
        node.state_      = PrecisionTimerState::IDLE;
    }

    hal::CpuClockDeadline PrecisionTimerEngine::root_deadline() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return root_deadline_locked();
    }

    PrecisionTimerState PrecisionTimerEngine::state(const PrecisionTimerNode &node) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return node.state_;
    }

    size_t PrecisionTimerEngine::size() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return heap_.size();
    }
}  // namespace kernel::timer
