/**
 * @file scheduler.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Thread 事件层、RunQueue 状态提交与上下文切换。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <arch/timer.h>
#include <log.h>
#include <obj/process.h>
#include <scheduler/adapter.h>
#include <scheduler/scheduler.h>
#include <synchronized.h>

#include <type_traits>
#include <utility>

namespace scheduler {
    namespace {
        constinit SchedulerCore scheduler;

        enum class RunnableNotificationAction : u8_t {
            ALREADY_RUNNABLE,
            RECORD_PENDING,
            ENQUEUE,
            INVALID,
        };

        [[nodiscard]] constexpr RunnableNotificationAction notification_action(
            task::ThreadState state) noexcept {
            switch (state) {
                case task::ThreadState::RUNNING:
                case task::ThreadState::READY:    return RunnableNotificationAction::ALREADY_RUNNABLE;
                case task::ThreadState::BLOCKING: return RunnableNotificationAction::RECORD_PENDING;
                case task::ThreadState::BLOCKED:  return RunnableNotificationAction::ENQUEUE;
                default:                          return RunnableNotificationAction::INVALID;
            }
        }

        static_assert(notification_action(task::ThreadState::RUNNING) ==
                      RunnableNotificationAction::ALREADY_RUNNABLE);
        static_assert(notification_action(task::ThreadState::BLOCKING) ==
                      RunnableNotificationAction::RECORD_PENDING);
        static_assert(notification_action(task::ThreadState::BLOCKED) ==
                      RunnableNotificationAction::ENQUEUE);
    }  // namespace

    static_assert(!std::is_copy_constructible_v<BlockToken>);
    static_assert(!std::is_copy_assignable_v<BlockToken>);
    static_assert(std::is_nothrow_move_constructible_v<BlockToken>);
    static_assert(std::is_nothrow_move_assignable_v<BlockToken>);

    BlockToken::BlockToken(BlockToken &&other) noexcept
        : thread_(other.thread_), sequence_(other.sequence_) {
        other.invalidate();
    }

    BlockToken &BlockToken::operator=(BlockToken &&other) noexcept {
        if (this == &other)
            return *this;
        if (thread_ != nullptr)
            kernel::log::panic("overwriting an active scheduler BlockToken");
        thread_   = other.thread_;
        sequence_ = other.sequence_;
        other.invalidate();
        return *this;
    }

    BlockToken::~BlockToken() noexcept {
        if (thread_ != nullptr)
            kernel::log::panic("scheduler BlockToken was neither committed nor cancelled");
    }

    void BlockToken::invalidate() noexcept {
        thread_   = nullptr;
        sequence_ = 0;
    }

    SchedulerCore &instance() noexcept {
        return scheduler;
    }

    units::time SchedulerCore::now() const noexcept {
        auto &clock = hal::CpuClock::instance();
        return clock.available() ? clock.current_time() : units::time{};
    }

    task::Thread *SchedulerCore::current() const noexcept {
        return run_queue_.current == nullptr ? nullptr
                                             : &ThreadSchedAdapter::owner(*run_queue_.current);
    }

    task::Thread &SchedulerCore::current_thread() const noexcept {
        auto *thread = current();
        if (thread == nullptr)
            kernel::log::panic("scheduler has no current Thread");
        return *thread;
    }

    tay::expected<void, SchedulerError> SchedulerCore::initialize(
        task::Thread &bootstrap) noexcept {
        if (ready_)
            return tay::Err(SchedulerError::AlreadyReady());
        if (run_queue_.current != nullptr)
            return tay::Err(SchedulerError::QueueStateMismatch(run_queue_.current->queue_state));
        if (bootstrap.state_ != task::ThreadState::RUNNING)
            return tay::Err(SchedulerError::BootstrapNotRunning(bootstrap.state_));
        if (!bootstrap.scheduler_attached_)
            return tay::Err(SchedulerError::ThreadNotAttached());
        if (hal::interrupts_enabled())
            return tay::Err(SchedulerError::InterruptsEnabled());

        auto &entity = ThreadSchedAdapter::entity(bootstrap);
        if (entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr)
            return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
        const auto timestamp = now();
        entity.queue_state   = QueueState::CURRENT;
        entity.cpu           = run_queue_.cpu;
        run_queue_.current   = &entity;
        accounting_.on_enqueue(bootstrap.scheduler_storage_.statistics, timestamp);
        accounting_.on_dispatch(bootstrap.scheduler_storage_.statistics, timestamp);
        ++run_queue_.transition_generation;
        ready_ = true;
        refresh_preemption_deadline_locked(timestamp);
        return {};
    }

    tay::expected<void, SchedulerError> SchedulerCore::attach(task::Thread &thread) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_)
            return tay::Err(SchedulerError::NotReady());
        if (thread.scheduler_attached_)
            return tay::Err(SchedulerError::ThreadAlreadyAttached());
        if (!thread.configured_)
            return tay::Err(SchedulerError::ThreadNotConfigured());
        if (thread.state_ != task::ThreadState::CREATED &&
            thread.state_ != task::ThreadState::SUSPENDED)
            return tay::Err(SchedulerError::InvalidThreadState(thread.state_));
        if (!thread.process_->submitted())
            return tay::Err(SchedulerError::ThreadNotSubmitted());

        auto &entity = ThreadSchedAdapter::entity(thread);
        if (entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr)
            return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));

        const auto timestamp       = now();
        thread.state_              = task::ThreadState::READY;
        thread.scheduler_attached_ = true;
        thread.scheduler_ref_      = cap::ObjectRef<task::Thread>(thread);
        accounting_.on_enqueue(thread.scheduler_storage_.statistics, timestamp);
        const auto entered = class_set_.enter(
            run_queue_, entity, EnterContext{.reason = EnterReason::ADMIT, .now = timestamp});
        if (entered.should_preempt)
            mark_need_resched_locked(SelectReason::WAKE);
        refresh_preemption_deadline_locked(timestamp);
        return tay::Ok();
    }

    tay::expected<void, SchedulerError> SchedulerCore::detach(task::Thread &thread,
                                                              DetachReason reason) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_)
            return tay::Err(SchedulerError::NotReady());
        if (!thread.scheduler_attached_)
            return tay::Err(SchedulerError::ThreadNotAttached());
        if (ThreadSchedAdapter::entity(thread).class_type == ClassType::IDLE)
            return tay::Err(SchedulerError::IdleThreadOperation());
        if (!thread.timed_wait_idle())
            return tay::Err(SchedulerError::TimedWaitActive());

        auto &entity = ThreadSchedAdapter::entity(thread);
        if (thread.state_ == task::ThreadState::READY) {
            if (entity.queue_state != QueueState::QUEUED || entity.run_queue != &run_queue_)
                return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
            class_set_.leave(
                run_queue_, entity,
                LeaveContext{.reason = reason == DetachReason::TERMINATE ? LeaveReason::TERMINATE
                                                                         : LeaveReason::SUSPEND,
                             .now    = now()});
        } else if (thread.state_ != task::ThreadState::BLOCKED) {
            return tay::Err(SchedulerError::InvalidThreadState(thread.state_));
        } else if (entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr) {
            return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
        }

        thread.state_              = reason == DetachReason::TERMINATE ? task::ThreadState::EXITED
                                                                       : task::ThreadState::SUSPENDED;
        thread.scheduler_attached_ = false;
        thread.wake_pending_       = false;
        thread.scheduler_ref_.reset();
        refresh_preemption_deadline_locked(now());
        return {};
    }

    void SchedulerCore::wake(task::Thread &thread, WakeReason) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_ || !thread.scheduler_attached_)
            kernel::log::panic("attempted to wake a detached Thread");

        if (thread.state_ == task::ThreadState::BLOCKING) {
            auto &entity = ThreadSchedAdapter::entity(thread);
            if (run_queue_.current != &entity || entity.queue_state != QueueState::CURRENT ||
                entity.run_queue != nullptr || !thread.block_token_active_)
                kernel::log::panic("blocking Thread has no active park transition");
            thread.wake_pending_ = true;
            return;
        }
        if (thread.state_ != task::ThreadState::BLOCKED)
            kernel::log::panic("attempted to wake a Thread that is not blocked");

        wake_blocked_locked(thread, now());
    }

    void SchedulerCore::notify_runnable_work(task::Thread &thread) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_ || !thread.scheduler_attached_)
            kernel::log::panic("attempted to notify a detached Thread");

        auto &entity = ThreadSchedAdapter::entity(thread);
        switch (notification_action(thread.state_)) {
            case RunnableNotificationAction::ALREADY_RUNNABLE:
                if (thread.state_ == task::ThreadState::RUNNING) {
                    if (run_queue_.current != &entity ||
                        entity.queue_state != QueueState::CURRENT || entity.run_queue != nullptr)
                        kernel::log::panic("running Thread has inconsistent run queue membership");
                } else if (entity.queue_state != QueueState::QUEUED ||
                           entity.run_queue != &run_queue_)
                {
                    kernel::log::panic("ready Thread has inconsistent run queue membership");
                }
                return;
            case RunnableNotificationAction::RECORD_PENDING:
                if (run_queue_.current != &entity || entity.queue_state != QueueState::CURRENT ||
                    entity.run_queue != nullptr || !thread.block_token_active_)
                    kernel::log::panic("blocking Thread has no active park transition");
                thread.wake_pending_ = true;
                return;
            case RunnableNotificationAction::ENQUEUE: wake_blocked_locked(thread, now()); return;
            case RunnableNotificationAction::INVALID:
                kernel::log::panic("attempted to notify a non-runnable Thread lifecycle state");
        }
    }

    void SchedulerCore::wake_blocked_locked(task::Thread &thread, units::time timestamp) noexcept {
        auto &entity = ThreadSchedAdapter::entity(thread);
        if (thread.state_ != task::ThreadState::BLOCKED || thread.block_token_active_ ||
            entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr)
            kernel::log::panic("blocked Thread has stale scheduler membership");

        thread.state_ = task::ThreadState::READY;
        accounting_.on_enqueue(thread.scheduler_storage_.statistics, timestamp);
        const auto entered = class_set_.enter(
            run_queue_, entity, EnterContext{.reason = EnterReason::WAKE, .now = timestamp});
        if (entered.should_preempt)
            mark_need_resched_locked(SelectReason::WAKE);
        refresh_preemption_deadline_locked(timestamp);
    }

    SelectResult SchedulerCore::select_locked(SelectReason reason, units::time timestamp,
                                              bool force) noexcept {
        units::duration current_runtime{};
        if (run_queue_.current != nullptr) {
            const auto &statistics =
                ThreadSchedAdapter::owner(*run_queue_.current).scheduler_storage_.statistics;
            current_runtime = timestamp >= statistics.last_start ? timestamp - statistics.last_start
                                                                 : units::duration{};
        }
        return class_set_.select(run_queue_, SelectContext{.reason          = reason,
                                                           .now             = timestamp,
                                                           .current         = run_queue_.current,
                                                           .current_runtime = current_runtime,
                                                           .force           = force});
    }

    SchedEntity &SchedulerCore::take_candidate_locked(const SelectResult &selection,
                                                      units::time timestamp) noexcept {
        auto *candidate = selection.candidate;
        if (candidate == nullptr)
            kernel::log::panic("scheduler selection has no runnable candidate or idle Thread");
        if (candidate == run_queue_.idle) {
            if (candidate->queue_state != QueueState::DETACHED || candidate->run_queue != nullptr)
                kernel::log::panic("idle Thread has ready queue membership");
            return *candidate;
        }
        if (candidate->queue_state != QueueState::QUEUED || candidate->run_queue != &run_queue_)
            kernel::log::panic("scheduler selected an entity outside the local ready queue");
        class_set_.leave(run_queue_, *candidate,
                         LeaveContext{.reason = LeaveReason::DISPATCH, .now = timestamp});
        return *candidate;
    }

    void SchedulerCore::publish_current_locked(SchedEntity &entity,
                                               units::time timestamp) noexcept {
        if (run_queue_.current != nullptr || entity.queue_state != QueueState::DETACHED ||
            entity.run_queue != nullptr)
            kernel::log::panic("cannot publish scheduler current slot");
        // 上一个 current 的 deadline 不得被新实体继承；新 deadline 随后按新实体的剩余时间片发布。
        publish_preemption_deadline_locked(hal::CpuClockDeadline::disarmed());
        auto &thread       = ThreadSchedAdapter::owner(entity);
        entity.queue_state = QueueState::CURRENT;
        entity.cpu         = run_queue_.cpu;
        run_queue_.current = &entity;
        thread.state_      = task::ThreadState::RUNNING;
        accounting_.on_dispatch(thread.scheduler_storage_.statistics, timestamp);
        ++run_queue_.transition_generation;
        refresh_preemption_deadline_locked(timestamp);
    }

    void SchedulerCore::consume_current_slice_locked(task::Thread &thread, units::time timestamp,
                                                     SelectReason reason) noexcept {
        auto &entity = ThreadSchedAdapter::entity(thread);
        if (entity.class_type != ClassType::RR)
            return;
        auto &rr = thread.scheduler_storage_.policy.get<RrEntity>();
        if (reason == SelectReason::YIELD || reason == SelectReason::TICK) {
            rr.remaining_slice = RR_TIME_SLICE;
            return;
        }
        if (!preemption_deadline_.armed) {
            rr.remaining_slice = RR_TIME_SLICE;
            return;
        }
        const auto deadline = preemption_deadline_.when;
        rr.remaining_slice  = timestamp >= deadline ? units::duration{} : deadline - timestamp;
    }

    SchedulerCore::SwitchPair SchedulerCore::switch_runnable_current_locked(
        EnterReason enter_reason, SelectReason select_reason, units::time timestamp) noexcept {
        if (run_queue_.current == nullptr)
            kernel::log::panic("cannot reschedule without a current Thread");
        auto *previous = &ThreadSchedAdapter::owner(*run_queue_.current);
        auto selection = select_locked(select_reason, timestamp, true);
        if (selection.action == SelectAction::KEEP_CURRENT) {
            run_queue_.clear_flag(RunQueueFlags::NEED_RESCHED);
            return SwitchPair{.previous = previous, .next = previous};
        }

        auto &candidate = take_candidate_locked(selection, timestamp);
        auto &entity    = ThreadSchedAdapter::entity(*previous);
        if (entity.queue_state != QueueState::CURRENT || run_queue_.current != &entity)
            kernel::log::panic("current Thread has inconsistent queue state");

        const auto elapsed =
            accounting_.on_stop(previous->scheduler_storage_.statistics, timestamp);
        consume_current_slice_locked(*previous, timestamp, select_reason);
        run_queue_.current = nullptr;
        entity.queue_state = QueueState::DETACHED;
        previous->state_   = task::ThreadState::READY;
        ++run_queue_.transition_generation;

        if (entity.class_type == ClassType::RR) {
            accounting_.on_enqueue(previous->scheduler_storage_.statistics, timestamp);
            (void)class_set_.enter(
                run_queue_, entity,
                EnterContext{
                    .reason = enter_reason, .now = timestamp, .unaccounted_runtime = elapsed});
        } else if (entity.class_type != ClassType::IDLE || run_queue_.idle != &entity) {
            kernel::log::panic("current Thread has an unsupported scheduling class");
        }

        run_queue_.clear_flag(RunQueueFlags::NEED_RESCHED);
        publish_current_locked(candidate, timestamp);
        if (select_reason == SelectReason::YIELD)
            ++previous->scheduler_storage_.statistics.voluntary_switches;
        else
            ++previous->scheduler_storage_.statistics.involuntary_switches;
        return SwitchPair{.previous = previous, .next = &ThreadSchedAdapter::owner(candidate)};
    }

    tay::expected<BlockToken, SchedulerError> SchedulerCore::prepare_block_current() noexcept {
        if (hal::interrupts_enabled())
            return tay::Err(SchedulerError::InterruptsEnabled());

        tay::lock_guard guard(run_queue_.lock);
        if (!ready_)
            return tay::Err(SchedulerError::NotReady());
        if (run_queue_.current == nullptr)
            return tay::Err(SchedulerError::NoRunnableThread());
        auto &thread = ThreadSchedAdapter::owner(*run_queue_.current);
        auto &entity = ThreadSchedAdapter::entity(thread);
        if (thread.state_ != task::ThreadState::RUNNING)
            return tay::Err(SchedulerError::InvalidThreadState(thread.state_));
        if (!thread.scheduler_attached_)
            return tay::Err(SchedulerError::ThreadNotAttached());
        if (thread.block_token_active_ || thread.wake_pending_)
            return tay::Err(SchedulerError::InvalidBlockToken());
        if (entity.queue_state != QueueState::CURRENT || entity.run_queue != nullptr)
            return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
        if (entity.class_type == ClassType::IDLE)
            return tay::Err(SchedulerError::IdleThreadOperation());

        ++thread.block_sequence_;
        if (thread.block_sequence_ == 0)
            ++thread.block_sequence_;
        thread.block_token_active_ = true;
        thread.state_              = task::ThreadState::BLOCKING;
        return BlockToken(thread, thread.block_sequence_);
    }

    void SchedulerCore::validate_block_token_locked(const BlockToken &token) const noexcept {
        if (token.thread_ == nullptr || token.sequence_ == 0)
            kernel::log::panic("attempted to reuse a consumed scheduler BlockToken");
        const auto &thread = *token.thread_;
        const auto &entity = ThreadSchedAdapter::entity(thread);
        if (!ready_ || run_queue_.current != &entity ||
            thread.state_ != task::ThreadState::BLOCKING || !thread.scheduler_attached_ ||
            !thread.block_token_active_ || thread.block_sequence_ != token.sequence_ ||
            entity.queue_state != QueueState::CURRENT || entity.run_queue != nullptr ||
            entity.class_type == ClassType::IDLE)
            kernel::log::panic("scheduler BlockToken owner or transition is no longer current");
    }

    tay::expected<void, SchedulerError> SchedulerCore::commit_block_current(
        BlockToken &&token) noexcept {
        if (hal::interrupts_enabled())
            kernel::log::panic("committing a scheduler BlockToken requires local IRQs disabled");

        task::Thread *previous = nullptr;
        task::Thread *next     = nullptr;
        {
            tay::lock_guard guard(run_queue_.lock);
            validate_block_token_locked(token);
            previous                      = token.thread_;
            previous->block_token_active_ = false;
            token.invalidate();
            if (previous->wake_pending_) {
                previous->wake_pending_ = false;
                previous->state_        = task::ThreadState::RUNNING;
                return {};
            }

            const auto timestamp = now();
            auto selection =
                class_set_.select(run_queue_, SelectContext{.reason  = SelectReason::BLOCK,
                                                            .now     = timestamp,
                                                            .current = nullptr,
                                                            .force   = true});
            if (selection.candidate == nullptr) {
                previous->state_ = task::ThreadState::RUNNING;
                refresh_preemption_deadline_locked(timestamp);
                return tay::Err(SchedulerError::NoRunnableThread());
            }
            auto &candidate = take_candidate_locked(selection, timestamp);
            (void)accounting_.on_stop(previous->scheduler_storage_.statistics, timestamp);
            if (ThreadSchedAdapter::entity(*previous).class_type == ClassType::RR)
                previous->scheduler_storage_.policy.get<RrEntity>().remaining_slice = RR_TIME_SLICE;
            run_queue_.current = nullptr;
            auto &entity       = ThreadSchedAdapter::entity(*previous);
            entity.queue_state = QueueState::DETACHED;
            previous->state_   = task::ThreadState::BLOCKED;
            ++previous->scheduler_storage_.statistics.voluntary_switches;
            ++run_queue_.transition_generation;
            publish_current_locked(candidate, timestamp);
            next = &ThreadSchedAdapter::owner(candidate);
        }
        if (previous != next)
            switch_to(*previous, *next);
        return {};
    }

    void SchedulerCore::cancel_block_current(BlockToken &&token) noexcept {
        if (hal::interrupts_enabled())
            kernel::log::panic("cancelling a scheduler BlockToken requires local IRQs disabled");

        tay::lock_guard guard(run_queue_.lock);
        validate_block_token_locked(token);
        auto &thread               = *token.thread_;
        thread.block_token_active_ = false;
        thread.wake_pending_       = false;
        thread.state_              = task::ThreadState::RUNNING;
        token.invalidate();
        refresh_preemption_deadline_locked(now());
    }

    tay::expected<void, SchedulerError> SchedulerCore::block_current() noexcept {
        hal::interrupt_guard interrupt_guard;
        return commit_block_current(TAY_TRY(prepare_block_current()));
    }

    void SchedulerCore::yield_current() noexcept {
        hal::interrupt_guard interrupt_guard;
        SwitchPair switch_pair{};
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || run_queue_.current == nullptr ||
                current_thread().state_ != task::ThreadState::RUNNING)
                kernel::log::panic("invalid scheduler yield");
            switch_pair =
                switch_runnable_current_locked(EnterReason::YIELD, SelectReason::YIELD, now());
        }
        if (switch_pair.required())
            switch_to(*switch_pair.previous, *switch_pair.next);
    }

    tay::expected<void, SchedulerError> SchedulerCore::install_preemption_deadline_sink(
        PreemptionDeadlineSink sink) noexcept {
        if (sink.publish == nullptr)
            return tay::Err(SchedulerError::InvalidDeadlineSink());
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (deadline_sink_.publish != nullptr)
            return tay::Err(SchedulerError::DeadlineSinkAlreadyInstalled());
        deadline_sink_ = sink;
        deadline_sink_.publish(deadline_sink_.context, preemption_deadline_);
        return {};
    }

    void SchedulerCore::publish_preemption_deadline_locked(
        hal::CpuClockDeadline deadline) noexcept {
        if (preemption_deadline_ == deadline)
            return;
        preemption_deadline_ = deadline;
        if (deadline_sink_.publish != nullptr)
            deadline_sink_.publish(deadline_sink_.context, deadline);
    }

    void SchedulerCore::set_preemption_deadline(units::time deadline) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        publish_preemption_deadline_locked(hal::CpuClockDeadline::at(deadline));
    }

    void SchedulerCore::clear_preemption_deadline() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        publish_preemption_deadline_locked(hal::CpuClockDeadline::disarmed());
    }

    hal::CpuClockDeadline SchedulerCore::preemption_deadline() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        return preemption_deadline_;
    }

    void SchedulerCore::refresh_preemption_deadline_locked(units::time timestamp) noexcept {
        if (!ready_ || run_queue_.current == nullptr ||
            run_queue_.current->class_type != ClassType::RR || run_queue_.queued_count == 0 ||
            run_queue_.has_flag(RunQueueFlags::NEED_RESCHED) ||
            !hal::CpuClock::instance().available())
        {
            if (run_queue_.current != nullptr && run_queue_.current->class_type == ClassType::RR &&
                run_queue_.queued_count == 0)
                ThreadSchedAdapter::owner(*run_queue_.current)
                    .scheduler_storage_.policy.get<RrEntity>()
                    .remaining_slice = RR_TIME_SLICE;
            publish_preemption_deadline_locked(hal::CpuClockDeadline::disarmed());
            return;
        }

        auto &thread = ThreadSchedAdapter::owner(*run_queue_.current);
        auto &rr     = thread.scheduler_storage_.policy.get<RrEntity>();
        if (preemption_deadline_.armed)
            return;
        if (rr.remaining_slice == units::duration{})
            rr.remaining_slice = RR_TIME_SLICE;
        publish_preemption_deadline_locked(
            hal::CpuClockDeadline::at(timestamp + rr.remaining_slice));
    }

    void SchedulerCore::request_preemption(SelectReason reason) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_ || run_queue_.current == nullptr)
            return;
        auto &thread = ThreadSchedAdapter::owner(*run_queue_.current);
        if (ThreadSchedAdapter::entity(thread).class_type != ClassType::RR ||
            run_queue_.queued_count == 0)
        {
            refresh_preemption_deadline_locked(now());
            return;
        }
        thread.scheduler_storage_.policy.get<RrEntity>().remaining_slice = units::duration{};
        publish_preemption_deadline_locked(hal::CpuClockDeadline::disarmed());
        mark_need_resched_locked(reason);
    }

    void SchedulerCore::mark_need_resched_locked(SelectReason reason) noexcept {
        run_queue_.set_flag(RunQueueFlags::NEED_RESCHED);
        run_queue_.resched_reason = reason;
    }

    void SchedulerCore::request_reschedule(CpuId cpu) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (cpu != run_queue_.cpu)
            kernel::log::panic("remote reschedule requires scheduler phase S5");
        mark_need_resched_locked(SelectReason::RESCHEDULE_IPI);
    }

    void SchedulerCore::schedule() noexcept {
        hal::interrupt_guard interrupt_guard;
        SwitchPair switch_pair{};
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || !run_queue_.has_flag(RunQueueFlags::NEED_RESCHED) ||
                run_queue_.current == nullptr)
                return;
            switch_pair = switch_runnable_current_locked(EnterReason::PREEMPT,
                                                         run_queue_.resched_reason, now());
        }
        if (switch_pair.required())
            switch_to(*switch_pair.previous, *switch_pair.next);
    }

    void SchedulerCore::become_idle() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_ || run_queue_.current == nullptr || run_queue_.idle != nullptr)
            kernel::log::panic("invalid idle Thread installation");
        auto &thread = ThreadSchedAdapter::owner(*run_queue_.current);
        auto &entity = ThreadSchedAdapter::entity(thread);
        if (thread.state_ != task::ThreadState::RUNNING ||
            entity.queue_state != QueueState::CURRENT)
            kernel::log::panic("idle installation requires the current Thread");
        initialize_policy(thread.scheduler_storage_, ClassType::IDLE);
        run_queue_.idle = &entity;
        if (run_queue_.queued_count != 0)
            mark_need_resched_locked(SelectReason::WAKE);
        refresh_preemption_deadline_locked(now());
    }

    [[noreturn]] void SchedulerCore::exit_current() noexcept {
        hal::disable_interrupts();
        task::Thread *previous = nullptr;
        task::Thread *next     = nullptr;
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || run_queue_.current == nullptr)
                kernel::log::panic("invalid Thread exit");
            previous     = &ThreadSchedAdapter::owner(*run_queue_.current);
            auto &entity = ThreadSchedAdapter::entity(*previous);
            if (previous->state_ != task::ThreadState::RUNNING ||
                entity.class_type == ClassType::IDLE)
                kernel::log::panic("invalid Thread exit state");

            const auto timestamp = now();
            auto selection =
                class_set_.select(run_queue_, SelectContext{.reason  = SelectReason::EXIT,
                                                            .now     = timestamp,
                                                            .current = nullptr,
                                                            .force   = true});
            auto &candidate = take_candidate_locked(selection, timestamp);
            (void)accounting_.on_stop(previous->scheduler_storage_.statistics, timestamp);
            run_queue_.current            = nullptr;
            entity.queue_state            = QueueState::DETACHED;
            previous->state_              = task::ThreadState::EXITED;
            previous->scheduler_attached_ = false;
            ++previous->scheduler_storage_.statistics.voluntary_switches;
            ++run_queue_.transition_generation;
            if (deferred_exit_)
                kernel::log::panic("存在尚未回收的退出 Thread");
            deferred_exit_ = std::move(previous->scheduler_ref_);
            publish_current_locked(candidate, timestamp);
            next = &ThreadSchedAdapter::owner(candidate);
        }
        switch_to(*previous, *next);
        kernel::log::panic("exited Thread resumed");
    }

    void SchedulerCore::switch_to(task::Thread &previous, task::Thread &next) noexcept {
        if (hal::interrupts_enabled())
            kernel::log::panic("context switch requires local interrupts disabled");
        next.process_->activate_address_space();
        if (&previous == &next)
            return;
        hal::__switch_to(&previous.context_, &next.context_);
        // 退出线程把最后一个 scheduler 引用交给下一条可运行线程；此处已经运行在
        // 恢复线程的栈上，可以安全销毁退出线程的 TCB 与内核栈。
        deferred_exit_.reset();
        if (run_queue_.current != &ThreadSchedAdapter::entity(previous) ||
            previous.state_ != task::ThreadState::RUNNING)
            kernel::log::panic("Thread resumed with inconsistent scheduler state");
    }

    [[noreturn]] void SchedulerCore::bootstrap_current() noexcept {
        auto *thread = current();
        if (thread == nullptr)
            kernel::log::panic("invalid Thread bootstrap");
        deferred_exit_.reset();
        if (thread->mode_ == task::ThreadMode::USER)
            hal::enter_user(thread->user_frame_, thread->stack_.top());
        if (thread->entry_ == nullptr)
            kernel::log::panic("invalid kernel Thread bootstrap");
        // 新 kernel Thread 从 IRQ-off 的调度切换进入；普通 entry 必须恢复可抢占上下文。
        hal::enable_interrupts();
        thread->entry_(thread->argument_);
        exit_current();
    }

    void yield() noexcept {
        instance().yield_current();
    }

    [[noreturn]] void exit_current() noexcept {
        instance().exit_current();
    }

    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept {
        instance().bootstrap_current();
    }
}  // namespace scheduler
