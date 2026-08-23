/**
 * @file hrtimer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief precision timer arm/cancel/expire/post exactly-once 状态机。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <async/queue.h>
#include <async/worklet.h>
#include <log.h>
#include <smp/ipi.h>
#include <timer/hrtimer.h>

namespace kernel::timer {
    namespace {
        constinit HRTQueue bsp_engine;
    }  // namespace

    HRTQueue &bsp_hrtimers() noexcept {
        return bsp_engine;
    }

    void HRTQueue::initialize(DeadlineMux &deadlines) noexcept {
        auto state = state_.lock();
        if (state->deadlines != nullptr || state->deadline_cpu.value != cpu::INVALID_CPU ||
            !state->heap.empty())
            kernel::log::panic("invalid HRTQueue initialization");
        state->deadlines    = &deadlines;
        state->deadline_cpu = cpu::current_id();
    }

    hal::TimerDeadline HRTQueue::root_locked(const State &state) noexcept {
        const auto *root = state.heap.top();
        return root == nullptr
                   ? hal::TimerDeadline::disarmed()
                   : hal::TimerDeadline::at(units::time::from_nanoseconds(root->deadline_));
    }

    bool HRTQueue::sync_root_locked(State &state) noexcept {
        if (state.deadlines == nullptr || state.deadline_cpu.value == cpu::INVALID_CPU)
            kernel::log::panic("publishing an uninitialized precision timer root");
        if (state.deadline_cpu != cpu::current_id()) {
            state.root_deadline_dirty = true;
            if (++state.remote_deadline_generation == 0)
                kernel::log::panic("precision timer remote deadline generation 溢出");
            return true;
        }
        // engine lock -> DeadlineMux lock 是固定顺序；只有 owner CPU 允许触及 timer CSR。
        state.deadlines->publish_timer(root_locked(state));
        state.root_deadline_dirty = false;
        return false;
    }

    void HRTQueue::kick_owner(cpu::CpuId owner) noexcept {
        if (owner == cpu::current_id())
            return;
        if (auto requested = smp::request(owner, smp::IpiReason::TIMER_DEADLINE); !requested)
            kernel::log::panic("BSP precision timer deadline IPI failed: cpu={}, error={}",
                               owner.value, requested.error());
    }

    tay::expected<void, TimerError> HRTQueue::arm(HrTimer &node, units::time deadline,
                                                  async::Worklet &completion) noexcept {
        auto &queue = async::bsp_work_queue();
        if (!queue.accepting())
            return tay::Err(TimerError::WorkQueueNotAccepting());
        bool refresh_owner = false;
        cpu::CpuId owner{cpu::INVALID_CPU};
        {
            auto state = state_.lock();
            // setup() 必须先于 arm；IDLE -> RESERVED 的 release 发布 typed completion 字段，
            // WorkQueue 在 RESERVED -> QUEUED -> IDLE 的 acquire 链上观察这些字段。
            if (state->deadlines == nullptr)
                return tay::Err(TimerError::EngineNotInitialized());
            if (node.state_ != HRTState::IDLE)
                return tay::Err(TimerError::NodeNotIdle(node.state_));
            if (node.heap_hook_.linked || node.due_hook_.in_list)
                return tay::Err(TimerError::NodeAlreadyLinked());
            if (!completion.try_reserve_timer(queue))
                return tay::Err(TimerError::CompletionBusy());
            node.deadline_   = deadline.to_nanoseconds();
            node.completion_ = &completion;
            node.engine_     = this;
            state->heap.push(node);
            node.state_   = HRTState::QUEUED;
            refresh_owner = sync_root_locked(*state);
            owner         = state->deadline_cpu;
        }
        if (refresh_owner)
            kick_owner(owner);
        return {};
    }

    CancelResult HRTQueue::cancel(HrTimer &node) noexcept {
        bool refresh_owner = false;
        cpu::CpuId owner{cpu::INVALID_CPU};
        {
            auto state = state_.lock();
            if (node.engine_ != this || node.state_ == HRTState::IDLE ||
                node.state_ == HRTState::RETIRED)
                return CancelResult::NOT_ARMED;
            if (node.state_ != HRTState::QUEUED)
                return CancelResult::RACE_LOST;
            (void)state->heap.remove(node);
            node.state_   = HRTState::ELAPSED;
            refresh_owner = sync_root_locked(*state);
            owner         = state->deadline_cpu;
        }
        if (refresh_owner)
            kick_owner(owner);
        post_elapsed(node);
        return CancelResult::CANCELLED;
    }

    void HRTQueue::progress(units::time now) noexcept {
        timer_due_list due;
        const auto now_nanoseconds = now.to_nanoseconds();
        {
            auto state = state_.lock();
            if (state->deadlines == nullptr || state->deadline_cpu != cpu::current_id())
                kernel::log::panic("progressing an uninitialized HRTQueue");
            while (state->heap.top() != nullptr && state->heap.top()->deadline_ <= now_nanoseconds)
            {
                auto *node = state->heap.pop_min();
                if (node->state_ != HRTState::QUEUED)
                    kernel::log::panic("timer heap contained a non-queued node");
                node->state_ = HRTState::ELAPSED;
                due.push_back(node);
            }
            static_cast<void>(sync_root_locked(*state));
        }

        while (!due.empty()) {
            auto *node = due.pop_front();
            post_elapsed(*node);
        }
    }

    void HRTQueue::refresh_from_ipi() noexcept {
        auto state = state_.lock();
        if (state->deadlines == nullptr || state->deadline_cpu != cpu::current_id())
            kernel::log::panic("precision timer deadline IPI reached the wrong CPU");
        if (!state->root_deadline_dirty)
            return;
        const u64_t observed_generation = state->remote_deadline_generation;
        state->deadlines->publish_timer(root_locked(*state));
        // A remote publisher cannot change the generation while state_ is held. If it
        // publishes after unlock it sets dirty again and sends/coalesces a new IPI.
        if (observed_generation == state->remote_deadline_generation)
            state->root_deadline_dirty = false;
    }

    void HRTQueue::post_elapsed(HrTimer &node) noexcept {
        async::WorkQueue *queue    = nullptr;
        async::Worklet *completion = nullptr;
        {
            auto state = state_.lock();
            static_cast<void>(state);
            if (node.engine_ != this || node.state_ != HRTState::ELAPSED ||
                node.due_hook_.in_list || node.completion_ == nullptr)
                kernel::log::panic("posting an inconsistent elapsed timer node");
            node.state_ = HRTState::POSTED;
            queue       = &async::bsp_work_queue();
            completion  = node.completion_;
        }
        if (!queue->try_post_reserved(*completion))
            kernel::log::panic("fixed BSP WorkQueue rejected a timer completion");
    }

    void HRTQueue::retire(HrTimer &node) noexcept {
        auto state = state_.lock();
        static_cast<void>(state);
        if (node.engine_ != this || node.state_ != HRTState::POSTED || node.heap_hook_.linked ||
            node.due_hook_.in_list)
            kernel::log::panic("retiring an inconsistent posted timer node");
        node.state_ = HRTState::RETIRED;
    }

    void HRTQueue::reset(HrTimer &node) noexcept {
        auto state = state_.lock();
        static_cast<void>(state);
        if (node.engine_ != this || node.state_ != HRTState::RETIRED ||
            node.heap_hook_.linked || node.due_hook_.in_list)
            kernel::log::panic("resetting a timer node before retirement");
        node.deadline_   = 0;
        node.completion_ = nullptr;
        node.engine_     = nullptr;
        node.state_      = HRTState::IDLE;
    }

    hal::TimerDeadline HRTQueue::root_deadline() noexcept {
        auto state = state_.lock();
        return root_locked(*state);
    }

    HRTState HRTQueue::state(const HrTimer &node) noexcept {
        auto state = state_.lock();
        static_cast<void>(state);
        return node.state_;
    }

    size_t HRTQueue::size() noexcept {
        auto state = state_.lock();
        return state->heap.size();
    }
}  // namespace kernel::timer
