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
#include <cpu/topology.h>
#include <log.h>
#include <obj/process.h>
#include <scheduler/adapter.h>
#include <scheduler/scheduler.h>
#include <smp/ipi.h>
#include <synchronized.h>

#include <atomic>
#include <type_traits>
#include <utility>

namespace scheduler {
    namespace detail::placement {
        alignas(64) constinit SchedCore schedulers[cpu::MAX_CPUS];
        constinit std::atomic<u32_t> any_placement_cursor{0};

        enum class RunnableNotificationAction : u8_t {
            ALREADY_RUNNABLE,
            RECORD_PENDING,
            ENQUEUE,
            INVALID,
        };

        [[nodiscard]] constexpr RunnableNotificationAction notify_action(
            task::ThreadState state) noexcept {
            switch (state) {
                case task::ThreadState::RUNNING:
                case task::ThreadState::READY:    return RunnableNotificationAction::ALREADY_RUNNABLE;
                case task::ThreadState::BLOCKING: return RunnableNotificationAction::RECORD_PENDING;
                case task::ThreadState::BLOCKED:  return RunnableNotificationAction::ENQUEUE;
                default:                          return RunnableNotificationAction::INVALID;
            }
        }

        static_assert(notify_action(task::ThreadState::RUNNING) ==
                      RunnableNotificationAction::ALREADY_RUNNABLE);
        static_assert(notify_action(task::ThreadState::BLOCKING) ==
                      RunnableNotificationAction::RECORD_PENDING);
        static_assert(notify_action(task::ThreadState::BLOCKED) ==
                      RunnableNotificationAction::ENQUEUE);

        void kick_remote(cpu::CpuId target) noexcept {
            if (target == cpu::current_id())
                return;
            if (auto requested = smp::request(target, smp::IpiReason::RESCHEDULE); !requested)
                kernel::log::panic("remote scheduler reschedule IPI failed: cpu={}, error={}",
                                   target.value, requested.error());
        }

        [[nodiscard]] cpu::CpuId scheduler_owner(const task::Thread &thread) noexcept {
            const cpu::CpuId owner{thread.sched_cpu()};
            if (!owner.valid())
                kernel::log::panic("Thread 没有合法的 scheduler owner: cpu={}", owner.value);
            return owner;
        }

        [[nodiscard]] cpu::CpuId select_any_target(const cpu::CpuSnapshot &snapshot) noexcept {
            if (snapshot.online.empty())
                return {};

            const u32_t first =
                any_placement_cursor.fetch_add(1, std::memory_order_relaxed) % cpu::MAX_CPUS;
            cpu::CpuId selected{};
            size_t selected_load = static_cast<size_t>(-1);
            for (u32_t offset = 0; offset < cpu::MAX_CPUS; ++offset) {
                const auto candidate =
                    cpu::CpuId{(first + offset) % static_cast<u32_t>(cpu::MAX_CPUS)};
                if (!snapshot.online.test(candidate))
                    continue;
                const size_t load = for_cpu(candidate).debug_state().queued_count;
                if (load < selected_load) {
                    selected      = candidate;
                    selected_load = load;
                }
            }
            return selected;
        }
    }  // namespace detail::placement

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

    void SchedCore::bind_cpu(cpu::CpuId cpu) noexcept {
        if (run_queue_.cpu == cpu)
            return;
        if (ready_)
            kernel::log::panic("cannot rebind an initialized SchedCore");
        run_queue_.cpu = cpu;
    }

    SchedCore &for_cpu(cpu::CpuId cpu) noexcept {
        if (!cpu.valid())
            kernel::log::panic("invalid scheduler CPU id: {}", cpu.value);
        const auto snapshot = cpu::topology().snapshot();
        if (!snapshot.online.test(cpu))
            kernel::log::panic("scheduler CPU {} is not online", cpu.value);
        auto &core = detail::placement::schedulers[cpu.value];
        core.bind_cpu(cpu);
        return core;
    }

    SchedCore &prepare_cpu(cpu::CpuId cpu) noexcept {
        if (!cpu.valid())
            kernel::log::panic("invalid scheduler CPU id: {}", cpu.value);
        auto &core = detail::placement::schedulers[cpu.value];
        core.bind_cpu(cpu);
        return core;
    }

    SchedCore &local() noexcept {
        return for_cpu(cpu::current_id());
    }

    task::Thread *current() noexcept {
        return local().current();
    }

    tay::expected<void, SchedulerError> attach(task::Thread &thread, Placement placement) noexcept {
        cpu::CpuId target = placement.cpu;
        if (placement.kind == PlacementKind::ANY) {
            const auto snapshot = cpu::topology().snapshot();
            target              = detail::placement::select_any_target(snapshot);
            if (target.value == cpu::INVALID_CPU)
                return tay::Err(SchedulerError::NotReady());
        } else if (!cpu::topology().snapshot().online.test(target)) {
            return tay::Err(SchedulerError::NotReady());
        }
        return for_cpu(target).attach(thread);
    }

    void wake(task::Thread &thread, WakeReason reason) noexcept {
        for_cpu(detail::placement::scheduler_owner(thread)).wake(thread, reason);
    }

    void notify_work(task::Thread &thread) noexcept {
        for_cpu(detail::placement::scheduler_owner(thread)).notify_work(thread);
    }

    units::time SchedCore::now() const noexcept {
        auto &clock = hal::Clock::instance();
        return clock.available() ? clock.now() : units::time{};
    }

    task::Thread *SchedCore::current() const noexcept {
        auto *thread =
            run_queue_.current == nullptr ? nullptr : &ThreadSchedOps::owner(*run_queue_.current);
        if (run_queue_.cpu == cpu::current_id() && cpu::current_thread() != thread)
            kernel::log::panic("CpuLocal current Thread disagrees with local run queue");
        return thread;
    }

    task::Thread &SchedCore::current_thread() const noexcept {
        auto *thread = current();
        if (thread == nullptr)
            kernel::log::panic("scheduler has no current Thread");
        return *thread;
    }

    void SchedCore::set_state_locked(task::Thread &thread, task::ThreadState state) noexcept {
        thread.state_ = state;
        // state_ 的权威修改仍在 target run queue lock 内；release 快照只供远端诊断、等待者
        // 和生命周期轮询观察，绝不能作为无锁状态转换的依据。
        thread.sched_snapshot_.store(state, std::memory_order_release);
    }

    tay::expected<void, SchedulerError> SchedCore::initialize(task::Thread &bootstrap) noexcept {
        if (ready_)
            return tay::Err(SchedulerError::AlreadyReady());
        if (run_queue_.current != nullptr)
            return tay::Err(SchedulerError::QueueStateMismatch(run_queue_.current->queue_state));
        if (bootstrap.state_ != task::ThreadState::RUNNING)
            return tay::Err(SchedulerError::BootstrapNotRunning(bootstrap.state_));
        if (!bootstrap.sched_attached_)
            return tay::Err(SchedulerError::ThreadNotAttached());
        if (hal::irq_enabled())
            return tay::Err(SchedulerError::InterruptsEnabled());

        auto &entity      = ThreadSchedOps::entity(bootstrap);
        const u32_t owner = bootstrap.sched_cpu_.load(std::memory_order_acquire);
        if (owner == cpu::INVALID_CPU)
            bootstrap.sched_cpu_.store(run_queue_.cpu.value, std::memory_order_release);
        else if (owner != run_queue_.cpu.value)
            return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
        if (entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr)
            return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
        const auto timestamp = now();
        entity.queue_state   = QueueState::CURRENT;
        entity.cpu           = run_queue_.cpu;
        run_queue_.current   = &entity;
        accounting_.on_enqueue(bootstrap.sched_.statistics, timestamp);
        accounting_.on_dispatch(bootstrap.sched_.statistics, timestamp);
        ++run_queue_.transition_gen;
        ready_ = true;
        cpu::set_exec_owner(&bootstrap);
        refresh_preempt_locked(timestamp);
        return {};
    }

    tay::expected<void, SchedulerError> SchedCore::attach(task::Thread &thread) noexcept {
        hal::irq_guard irq_guard;
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_)
                return tay::Err(SchedulerError::NotReady());
            if (thread.sched_attached_)
                return tay::Err(SchedulerError::ThreadAlreadyAttached());
            if (!thread.configured_)
                return tay::Err(SchedulerError::ThreadNotConfigured());
            if (thread.state_ != task::ThreadState::CREATED &&
                thread.state_ != task::ThreadState::SUSPENDED)
                return tay::Err(SchedulerError::InvalidThreadState(thread.state_));
            if (!thread.process_->submitted())
                return tay::Err(SchedulerError::ThreadNotSubmitted());

            auto &entity = ThreadSchedOps::entity(thread);
            if (entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr)
                return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
            const u32_t owner = thread.sched_cpu_.load(std::memory_order_acquire);
            if (owner != cpu::INVALID_CPU && owner != run_queue_.cpu.value)
                return tay::Err(SchedulerError::QueueStateMismatch(entity.queue_state));
            if (owner == cpu::INVALID_CPU)
                thread.sched_cpu_.store(run_queue_.cpu.value, std::memory_order_release);

            const auto timestamp = now();
            set_state_locked(thread, task::ThreadState::READY);
            thread.sched_attached_ = true;
            thread.sched_ref_      = cap::KObjectRef<task::Thread>(thread);
            accounting_.on_enqueue(thread.sched_.statistics, timestamp);
            ++enqueue_count_;
            const auto entered = class_set_.enter(
                run_queue_, entity, EnterContext{.reason = EnterReason::ADMIT, .now = timestamp});
            if (entered.should_preempt)
                set_resched_locked(SelectReason::WAKE);
            refresh_preempt_locked(timestamp);
        }

        // 目标 run queue、Thread 状态与逻辑 deadline 已全部提交；只有退出该锁后才能通知 AP。
        detail::placement::kick_remote(run_queue_.cpu);
        return tay::Ok();
    }

    tay::expected<void, SchedulerError> SchedCore::detach(task::Thread &thread,
                                                          DetachReason reason) noexcept {
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_)
            return tay::Err(SchedulerError::NotReady());
        if (!thread.sched_attached_)
            return tay::Err(SchedulerError::ThreadNotAttached());
        if (ThreadSchedOps::entity(thread).class_type == ClassType::IDLE)
            return tay::Err(SchedulerError::IdleThreadOperation());
        if (!thread.wait_idle())
            return tay::Err(SchedulerError::TimedWaitActive());

        auto &entity = ThreadSchedOps::entity(thread);
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

        set_state_locked(thread, reason == DetachReason::TERMINATE ? task::ThreadState::EXITED
                                                                   : task::ThreadState::SUSPENDED);
        thread.sched_attached_ = false;
        thread.wake_pending_   = false;
        thread.sched_ref_.reset();
        refresh_preempt_locked(now());
        return {};
    }

    void SchedCore::wake(task::Thread &thread, WakeReason) noexcept {
        hal::irq_guard irq_guard;
        bool enqueued = false;
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || !thread.sched_attached_)
                kernel::log::panic("attempted to wake a detached Thread");

            if (thread.state_ == task::ThreadState::BLOCKING) {
                auto &entity = ThreadSchedOps::entity(thread);
                if (run_queue_.current != &entity || entity.queue_state != QueueState::CURRENT ||
                    entity.run_queue != nullptr || !thread.block_token_active_)
                    kernel::log::panic("blocking Thread has no active park transition");
                thread.wake_pending_ = true;
                return;
            }
            if (thread.state_ != task::ThreadState::BLOCKED)
                kernel::log::panic("attempted to wake a Thread that is not blocked");

            wake_locked(thread, now());
            enqueued = true;
        }

        if (enqueued)
            detail::placement::kick_remote(run_queue_.cpu);
    }

    void SchedCore::notify_work(task::Thread &thread) noexcept {
        hal::irq_guard irq_guard;
        bool enqueued = false;
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || !thread.sched_attached_)
                kernel::log::panic("attempted to notify a detached Thread");

            auto &entity = ThreadSchedOps::entity(thread);
            switch (detail::placement::notify_action(thread.state_)) {
                case detail::placement::RunnableNotificationAction::ALREADY_RUNNABLE:
                    if (thread.state_ == task::ThreadState::RUNNING) {
                        if (run_queue_.current != &entity ||
                            entity.queue_state != QueueState::CURRENT ||
                            entity.run_queue != nullptr)
                            kernel::log::panic(
                                "running Thread has inconsistent run queue membership");
                    } else if (entity.queue_state != QueueState::QUEUED ||
                               entity.run_queue != &run_queue_)
                    {
                        kernel::log::panic("ready Thread has inconsistent run queue membership");
                    }
                    return;
                case detail::placement::RunnableNotificationAction::RECORD_PENDING:
                    if (run_queue_.current != &entity ||
                        entity.queue_state != QueueState::CURRENT || entity.run_queue != nullptr ||
                        !thread.block_token_active_)
                        kernel::log::panic("blocking Thread has no active park transition");
                    thread.wake_pending_ = true;
                    return;
                case detail::placement::RunnableNotificationAction::ENQUEUE:
                    wake_locked(thread, now());
                    enqueued = true;
                    break;
                case detail::placement::RunnableNotificationAction::INVALID:
                    kernel::log::panic("attempted to notify a non-runnable Thread lifecycle state");
            }
        }

        if (enqueued)
            detail::placement::kick_remote(run_queue_.cpu);
    }

    void SchedCore::wake_locked(task::Thread &thread, units::time timestamp) noexcept {
        auto &entity = ThreadSchedOps::entity(thread);
        if (thread.state_ != task::ThreadState::BLOCKED || thread.block_token_active_ ||
            entity.queue_state != QueueState::DETACHED || entity.run_queue != nullptr)
            kernel::log::panic("blocked Thread has stale scheduler membership");

        set_state_locked(thread, task::ThreadState::READY);
        accounting_.on_enqueue(thread.sched_.statistics, timestamp);
        ++enqueue_count_;
        ++wake_count_;
        const auto entered = class_set_.enter(
            run_queue_, entity, EnterContext{.reason = EnterReason::WAKE, .now = timestamp});
        if (entered.should_preempt)
            set_resched_locked(SelectReason::WAKE);
        refresh_preempt_locked(timestamp);
    }

    SelectResult SchedCore::select_locked(SelectReason reason, units::time timestamp,
                                          bool force) noexcept {
        units::duration current_runtime{};
        if (run_queue_.current != nullptr) {
            const auto &statistics = ThreadSchedOps::owner(*run_queue_.current).sched_.statistics;
            current_runtime = timestamp >= statistics.last_start ? timestamp - statistics.last_start
                                                                 : units::duration{};
        }
        return class_set_.select(run_queue_, SelectContext{.reason          = reason,
                                                           .now             = timestamp,
                                                           .current         = run_queue_.current,
                                                           .current_runtime = current_runtime,
                                                           .force           = force});
    }

    SchedEntity &SchedCore::take_next_locked(const SelectResult &selection,
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

    void SchedCore::set_current_locked(SchedEntity &entity, units::time timestamp) noexcept {
        if (run_queue_.current != nullptr || entity.queue_state != QueueState::DETACHED ||
            entity.run_queue != nullptr)
            kernel::log::panic("cannot publish scheduler current slot");
        // 上一个 current 的 deadline 不得被新实体继承；新 deadline 随后按新实体的剩余时间片发布。
        publish_preempt_locked(hal::TimerDeadline::disarmed());
        auto &thread       = ThreadSchedOps::owner(entity);
        entity.queue_state = QueueState::CURRENT;
        entity.cpu         = run_queue_.cpu;
        run_queue_.current = &entity;
        // trap 入口通过 CpuLocal 取得用户 Thread 的 kernel stack；该发布与 current 提交
        // 同处 IRQ-off 临界区，避免返回用户态前观察到上一 Thread 的栈顶。stackless 的
        // adopted idle 保留 BSP 栈顶作为不可进入用户态的 fallback。
        if (thread.stack_.valid())
            cpu::local().kstack_top = thread.stack_.top();
        cpu::set_exec_owner(&thread);
        set_state_locked(thread, task::ThreadState::RUNNING);
        accounting_.on_dispatch(thread.sched_.statistics, timestamp);
        ++run_queue_.transition_gen;
        refresh_preempt_locked(timestamp);
    }

    void SchedCore::charge_rr_locked(task::Thread &thread, units::time timestamp,
                                        SelectReason reason) noexcept {
        auto &entity = ThreadSchedOps::entity(thread);
        if (entity.class_type != ClassType::RR)
            return;
        auto &rr = thread.sched_.policy.get<RrEntity>();
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

    SchedCore::SwitchPair SchedCore::switch_locked(EnterReason enter_reason,
                                                   SelectReason select_reason,
                                                   units::time timestamp) noexcept {
        if (run_queue_.current == nullptr)
            kernel::log::panic("cannot reschedule without a current Thread");
        auto *previous = &ThreadSchedOps::owner(*run_queue_.current);
        auto selection = select_locked(select_reason, timestamp, true);
        if (selection.action == SelectAction::KEEP_CURRENT) {
            run_queue_.clear_flag(RunQueueFlags::NEED_RESCHED);
            return SwitchPair{.previous = previous, .next = previous};
        }

        auto &candidate = take_next_locked(selection, timestamp);
        auto &entity    = ThreadSchedOps::entity(*previous);
        if (entity.queue_state != QueueState::CURRENT || run_queue_.current != &entity)
            kernel::log::panic("current Thread has inconsistent queue state");

        const auto elapsed = accounting_.on_stop(previous->sched_.statistics, timestamp);
        charge_rr_locked(*previous, timestamp, select_reason);
        run_queue_.current = nullptr;
        entity.queue_state = QueueState::DETACHED;
        set_state_locked(*previous, task::ThreadState::READY);
        ++run_queue_.transition_gen;

        if (entity.class_type == ClassType::RR) {
            accounting_.on_enqueue(previous->sched_.statistics, timestamp);
            ++enqueue_count_;
            (void)class_set_.enter(
                run_queue_, entity,
                EnterContext{
                    .reason = enter_reason, .now = timestamp, .unaccounted_runtime = elapsed});
        } else if (entity.class_type != ClassType::IDLE || run_queue_.idle != &entity) {
            kernel::log::panic("current Thread has an unsupported scheduling class");
        }

        run_queue_.clear_flag(RunQueueFlags::NEED_RESCHED);
        set_current_locked(candidate, timestamp);
        if (select_reason == SelectReason::YIELD) {
            ++yield_count_;
            ++previous->sched_.statistics.voluntary_switches;
        } else {
            if (select_reason == SelectReason::TICK)
                ++preemption_count_;
            ++previous->sched_.statistics.involuntary_switches;
        }
        return SwitchPair{.previous = previous, .next = &ThreadSchedOps::owner(candidate)};
    }

    tay::expected<BlockToken, SchedulerError> SchedCore::prepare_block() noexcept {
        if (hal::irq_enabled())
            return tay::Err(SchedulerError::InterruptsEnabled());

        tay::lock_guard guard(run_queue_.lock);
        if (!ready_)
            return tay::Err(SchedulerError::NotReady());
        if (run_queue_.current == nullptr)
            return tay::Err(SchedulerError::NoRunnableThread());
        auto &thread = ThreadSchedOps::owner(*run_queue_.current);
        auto &entity = ThreadSchedOps::entity(thread);
        if (thread.state_ != task::ThreadState::RUNNING)
            return tay::Err(SchedulerError::InvalidThreadState(thread.state_));
        if (!thread.sched_attached_)
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
        set_state_locked(thread, task::ThreadState::BLOCKING);
        return BlockToken(thread, thread.block_sequence_);
    }

    void SchedCore::check_block_locked(const BlockToken &token) const noexcept {
        if (token.thread_ == nullptr || token.sequence_ == 0)
            kernel::log::panic("attempted to reuse a consumed scheduler BlockToken");
        const auto &thread = *token.thread_;
        const auto &entity = ThreadSchedOps::entity(thread);
        if (!ready_ || run_queue_.current != &entity ||
            thread.state_ != task::ThreadState::BLOCKING || !thread.sched_attached_ ||
            !thread.block_token_active_ || thread.block_sequence_ != token.sequence_ ||
            entity.queue_state != QueueState::CURRENT || entity.run_queue != nullptr ||
            entity.class_type == ClassType::IDLE)
            kernel::log::panic("scheduler BlockToken owner or transition is no longer current");
    }

    tay::expected<void, SchedulerError> SchedCore::commit_block(BlockToken &&token) noexcept {
        if (hal::irq_enabled())
            kernel::log::panic("committing a scheduler BlockToken requires local IRQs disabled");

        task::Thread *previous = nullptr;
        task::Thread *next     = nullptr;
        {
            tay::lock_guard guard(run_queue_.lock);
            check_block_locked(token);
            previous                      = token.thread_;
            previous->block_token_active_ = false;
            token.invalidate();
            if (previous->wake_pending_) {
                previous->wake_pending_ = false;
                set_state_locked(*previous, task::ThreadState::RUNNING);
                return {};
            }

            const auto timestamp = now();
            auto selection =
                class_set_.select(run_queue_, SelectContext{.reason  = SelectReason::BLOCK,
                                                            .now     = timestamp,
                                                            .current = nullptr,
                                                            .force   = true});
            if (selection.candidate == nullptr) {
                set_state_locked(*previous, task::ThreadState::RUNNING);
                refresh_preempt_locked(timestamp);
                return tay::Err(SchedulerError::NoRunnableThread());
            }
            auto &candidate = take_next_locked(selection, timestamp);
            (void)accounting_.on_stop(previous->sched_.statistics, timestamp);
            if (ThreadSchedOps::entity(*previous).class_type == ClassType::RR)
                previous->sched_.policy.get<RrEntity>().remaining_slice = RR_TIME_SLICE;
            run_queue_.current = nullptr;
            auto &entity       = ThreadSchedOps::entity(*previous);
            entity.queue_state = QueueState::DETACHED;
            set_state_locked(*previous, task::ThreadState::BLOCKED);
            ++block_count_;
            ++previous->sched_.statistics.voluntary_switches;
            ++run_queue_.transition_gen;
            set_current_locked(candidate, timestamp);
            next = &ThreadSchedOps::owner(candidate);
            if (previous != next)
                ++context_switch_count_;
        }
        if (previous != next)
            switch_to(*previous, *next);
        return {};
    }

    void SchedCore::cancel_block(BlockToken &&token) noexcept {
        if (hal::irq_enabled())
            kernel::log::panic("cancelling a scheduler BlockToken requires local IRQs disabled");

        tay::lock_guard guard(run_queue_.lock);
        check_block_locked(token);
        auto &thread               = *token.thread_;
        thread.block_token_active_ = false;
        thread.wake_pending_       = false;
        set_state_locked(thread, task::ThreadState::RUNNING);
        token.invalidate();
        refresh_preempt_locked(now());
    }

    tay::expected<void, SchedulerError> SchedCore::block() noexcept {
        hal::irq_guard irq_guard;
        return commit_block(TAY_TRY(prepare_block()));
    }

    void SchedCore::yield() noexcept {
        hal::irq_guard irq_guard;
        SwitchPair switch_pair{};
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || run_queue_.current == nullptr ||
                current_thread().state_ != task::ThreadState::RUNNING)
                kernel::log::panic("invalid scheduler yield");
            switch_pair = switch_locked(EnterReason::YIELD, SelectReason::YIELD, now());
            if (switch_pair.required())
                ++context_switch_count_;
        }
        if (switch_pair.required())
            switch_to(*switch_pair.previous, *switch_pair.next);
    }

    tay::expected<void, SchedulerError> SchedCore::set_preempt_sink(PreemptSink sink) noexcept {
        if (sink.publish == nullptr)
            return tay::Err(SchedulerError::InvalidDeadlineSink());
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (deadline_sink_.publish != nullptr)
            return tay::Err(SchedulerError::PreemptSinkAlreadySet());
        deadline_sink_ = sink;
        deadline_sink_.publish(deadline_sink_.context, preemption_deadline_);
        run_queue_.clear_flag(RunQueueFlags::DEADLINE_DIRTY);
        return {};
    }

    void SchedCore::publish_preempt_locked(hal::TimerDeadline deadline) noexcept {
        if (preemption_deadline_ == deadline)
            return;
        preemption_deadline_ = deadline;
        if (run_queue_.cpu != cpu::current_id()) {
            // 目标队列由远端 waker 更新时，硬件 CSR 仍属于目标 CPU。RESCHEDULE IPI 在
            // release target queue lock 后送达，届时由目标用该已提交的逻辑 deadline 重编程。
            run_queue_.set_flag(RunQueueFlags::DEADLINE_DIRTY);
            return;
        }
        flush_preempt_locked();
    }

    void SchedCore::flush_preempt_locked() noexcept {
        if (run_queue_.cpu != cpu::current_id())
            kernel::log::panic("remote CPU attempted to program a scheduler deadline");
        if (deadline_sink_.publish == nullptr) {
            // bring-up 在安装 sink 前可以建立逻辑 deadline；安装时会以当前值完成首次发布。
            run_queue_.set_flag(RunQueueFlags::DEADLINE_DIRTY);
            return;
        }
        deadline_sink_.publish(deadline_sink_.context, preemption_deadline_);
        run_queue_.clear_flag(RunQueueFlags::DEADLINE_DIRTY);
    }

    hal::TimerDeadline SchedCore::preempt_deadline() noexcept {
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        return preemption_deadline_;
    }

    SchedDebug SchedCore::debug_state() noexcept {
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        return SchedDebug{
            .cpu                  = run_queue_.cpu,
            .ready                = ready_,
            .has_current          = run_queue_.current != nullptr,
            .idle_installed       = run_queue_.idle != nullptr,
            .need_reschedule      = run_queue_.has_flag(RunQueueFlags::NEED_RESCHED),
            .deadline_dirty       = run_queue_.has_flag(RunQueueFlags::DEADLINE_DIRTY),
            .queued_count         = run_queue_.queued_count,
            .transition_gen       = run_queue_.transition_gen,
            .enqueue_count        = enqueue_count_,
            .wake_count           = wake_count_,
            .block_count          = block_count_,
            .yield_count          = yield_count_,
            .preemption_count     = preemption_count_,
            .context_switch_count = context_switch_count_,
            .exit_count           = exit_count_,
        };
    }

    void SchedCore::refresh_preempt_locked(units::time timestamp) noexcept {
        if (!ready_ || run_queue_.current == nullptr ||
            run_queue_.current->class_type != ClassType::RR || run_queue_.queued_count == 0 ||
            run_queue_.has_flag(RunQueueFlags::NEED_RESCHED) || !hal::Clock::instance().available())
        {
            if (run_queue_.current != nullptr && run_queue_.current->class_type == ClassType::RR &&
                run_queue_.queued_count == 0)
                ThreadSchedOps::owner(*run_queue_.current)
                    .sched_.policy.get<RrEntity>()
                    .remaining_slice = RR_TIME_SLICE;
            publish_preempt_locked(hal::TimerDeadline::disarmed());
            return;
        }

        auto &thread = ThreadSchedOps::owner(*run_queue_.current);
        auto &rr     = thread.sched_.policy.get<RrEntity>();
        if (preemption_deadline_.armed)
            return;
        if (rr.remaining_slice == units::duration{})
            rr.remaining_slice = RR_TIME_SLICE;
        publish_preempt_locked(hal::TimerDeadline::at(timestamp + rr.remaining_slice));
    }

    void SchedCore::request_preempt(SelectReason reason) noexcept {
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_ || run_queue_.current == nullptr)
            return;
        auto &thread = ThreadSchedOps::owner(*run_queue_.current);
        if (ThreadSchedOps::entity(thread).class_type != ClassType::RR ||
            run_queue_.queued_count == 0)
        {
            refresh_preempt_locked(now());
            return;
        }
        thread.sched_.policy.get<RrEntity>().remaining_slice = units::duration{};
        publish_preempt_locked(hal::TimerDeadline::disarmed());
        set_resched_locked(reason);
    }

    void SchedCore::set_resched_locked(SelectReason reason) noexcept {
        run_queue_.set_flag(RunQueueFlags::NEED_RESCHED);
        run_queue_.resched_reason = reason;
    }

    void SchedCore::request_reschedule(CpuId cpu) noexcept {
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (cpu != run_queue_.cpu || cpu != cpu::current_id())
            kernel::log::panic("reschedule IPI dispatched on the wrong CPU");
        set_resched_locked(SelectReason::RESCHEDULE_IPI);
        refresh_preempt_locked(now());
        // 即使远端已把逻辑 deadline 写成相同值，也必须在目标 CPU 再次合并，避免
        // DEADLINE_DIRTY 与一个尚未被观察的旧 CSR deadline 脱节。
        flush_preempt_locked();
    }

    void SchedCore::schedule() noexcept {
        hal::irq_guard irq_guard;
        if (cpu::preempt_disabled()) {
            cpu::defer_resched();
            return;
        }
        SwitchPair switch_pair{};
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || !run_queue_.has_flag(RunQueueFlags::NEED_RESCHED) ||
                run_queue_.current == nullptr)
                return;
            switch_pair = switch_locked(EnterReason::PREEMPT, run_queue_.resched_reason, now());
            if (switch_pair.required())
                ++context_switch_count_;
        }
        if (switch_pair.required())
            switch_to(*switch_pair.previous, *switch_pair.next);
    }

    void SchedCore::become_idle() noexcept {
        hal::irq_guard irq_guard;
        tay::lock_guard guard(run_queue_.lock);
        if (!ready_ || run_queue_.current == nullptr || run_queue_.idle != nullptr)
            kernel::log::panic("invalid idle Thread installation");
        auto &thread = ThreadSchedOps::owner(*run_queue_.current);
        auto &entity = ThreadSchedOps::entity(thread);
        if (thread.state_ != task::ThreadState::RUNNING ||
            entity.queue_state != QueueState::CURRENT)
            kernel::log::panic("idle installation requires the current Thread");
        initialize_policy(thread.sched_, ClassType::IDLE);
        run_queue_.idle = &entity;
        if (run_queue_.queued_count != 0)
            set_resched_locked(SelectReason::WAKE);
        refresh_preempt_locked(now());
    }

    [[noreturn]] void SchedCore::exit() noexcept {
        hal::cli();
        task::Thread *previous = nullptr;
        task::Thread *next     = nullptr;
        {
            tay::lock_guard guard(run_queue_.lock);
            if (!ready_ || run_queue_.current == nullptr)
                kernel::log::panic("invalid Thread exit");
            previous     = &ThreadSchedOps::owner(*run_queue_.current);
            auto &entity = ThreadSchedOps::entity(*previous);
            if (previous->state_ != task::ThreadState::RUNNING ||
                entity.class_type == ClassType::IDLE)
                kernel::log::panic("invalid Thread exit state");

            const auto timestamp = now();
            auto selection =
                class_set_.select(run_queue_, SelectContext{.reason  = SelectReason::EXIT,
                                                            .now     = timestamp,
                                                            .current = nullptr,
                                                            .force   = true});
            auto &candidate = take_next_locked(selection, timestamp);
            (void)accounting_.on_stop(previous->sched_.statistics, timestamp);
            run_queue_.current = nullptr;
            entity.queue_state = QueueState::DETACHED;
            set_state_locked(*previous, task::ThreadState::EXITED);
            ++exit_count_;
            previous->sched_attached_ = false;
            ++previous->sched_.statistics.voluntary_switches;
            ++run_queue_.transition_gen;
            if (deferred_exit_)
                kernel::log::panic("存在尚未回收的退出 Thread");
            deferred_exit_ = std::move(previous->sched_ref_);
            set_current_locked(candidate, timestamp);
            next = &ThreadSchedOps::owner(candidate);
            ++context_switch_count_;
        }
        switch_to(*previous, *next);
        kernel::log::panic("exited Thread resumed");
    }

    void SchedCore::switch_to(task::Thread &previous, task::Thread &next) noexcept {
        if (hal::irq_enabled())
            kernel::log::panic("context switch requires local interrupts disabled");
        next.process_->activate_vm();
        if (&previous == &next)
            return;
        hal::__switch_to(&previous.context_, &next.context_);
        // 退出线程把最后一个 scheduler 引用交给下一条可运行线程；此处已经运行在
        // 恢复线程的栈上，可以安全销毁退出线程的 TCB 与内核栈。
        deferred_exit_.reset();
        if (run_queue_.current != &ThreadSchedOps::entity(previous) ||
            previous.state_ != task::ThreadState::RUNNING)
            kernel::log::panic("Thread resumed with inconsistent scheduler state");
    }

    [[noreturn]] void SchedCore::bootstrap() noexcept {
        auto *thread = current();
        if (thread == nullptr)
            kernel::log::panic("invalid Thread bootstrap");
        deferred_exit_.reset();
        if (thread->mode_ == task::ThreadMode::USER)
            hal::enter_user(thread->user_frame_, thread->stack_.top());
        if (thread->entry_ == nullptr)
            kernel::log::panic("invalid kernel Thread bootstrap");
        // 新 kernel Thread 从 IRQ-off 的调度切换进入；普通 entry 必须恢复可抢占上下文。
        hal::sti();
        thread->entry_(thread->argument_);
        exit();
    }

    void yield() noexcept {
        local().yield();
    }

    [[noreturn]] void exit() noexcept {
        local().exit();
    }

    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept {
        local().bootstrap();
    }

    extern "C" void kernel_preemption_checkpoint() noexcept {
        local().schedule();
    }
}  // namespace scheduler
