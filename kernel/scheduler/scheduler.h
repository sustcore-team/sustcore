/**
 * @file scheduler.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Thread 生命周期事件到 enter/leave/select 的单 CPU SchedulerCore。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/timer.h>
#include <obj/thread.h>
#include <scheduler/accounting.h>
#include <scheduler/error.h>
#include <scheduler/rq.h>
#include <tay/err.h>
#include <tay/expected.h>

namespace scheduler {
    /**
     * @brief Scheduler 向 BSP deadline coordinator 发布唯一的 RR deadline 来源。
     * @note 回调在本地 IRQ 关闭且持 run queue lock 时执行，必须有固定生命周期、不可阻塞、
     * 不可反向调用 scheduler；它只负责保存并重新合并硬件 one-shot deadline。
     */
    struct PreemptionDeadlineSink final {
        void *context                                                           = nullptr;
        void (*publish)(void *context, hal::CpuClockDeadline deadline) noexcept = nullptr;
    };

    /**
     * @brief 两阶段 park 的独占凭据。
     * @note token 有效期间调用方必须保持本地中断关闭，并且只能 commit 或 cancel 一次。
     */
    class BlockToken final {
    public:
        BlockToken(const BlockToken &)            = delete;
        BlockToken &operator=(const BlockToken &) = delete;
        BlockToken(BlockToken &&other) noexcept;
        BlockToken &operator=(BlockToken &&other) noexcept;
        ~BlockToken() noexcept;

    private:
        explicit BlockToken(task::Thread &thread, u64_t sequence) noexcept
            : thread_(&thread), sequence_(sequence) {}
        void invalidate() noexcept;

        task::Thread *thread_ = nullptr;
        u64_t sequence_       = 0;

        friend class SchedulerCore;
    };

    enum class DetachReason : u8_t {
        SUSPEND,
        TERMINATE,
    };

    enum class WakeReason : u8_t {
        EXPLICIT,
        WAIT_COMPLETED,
    };

    class SchedulerCore final {
    public:
        constexpr SchedulerCore() noexcept = default;

        SchedulerCore(const SchedulerCore &)            = delete;
        SchedulerCore &operator=(const SchedulerCore &) = delete;
        SchedulerCore(SchedulerCore &&)                 = delete;
        SchedulerCore &operator=(SchedulerCore &&)      = delete;

        [[nodiscard]] tay::expected<void, SchedulerError> initialize(
            task::Thread &bootstrap) noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> attach(task::Thread &thread) noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> detach(
            task::Thread &thread, DetachReason reason = DetachReason::SUSPEND) noexcept;
        void wake(task::Thread &thread, WakeReason reason = WakeReason::EXPLICIT) noexcept;
        void notify_runnable_work(task::Thread &thread) noexcept;
        [[nodiscard]] tay::expected<BlockToken, SchedulerError> prepare_block_current() noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> commit_block_current(
            BlockToken &&token) noexcept;
        void cancel_block_current(BlockToken &&token) noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> block_current() noexcept;

        void yield_current() noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> install_preemption_deadline_sink(
            PreemptionDeadlineSink sink) noexcept;
        void set_preemption_deadline(units::time deadline) noexcept;
        void clear_preemption_deadline() noexcept;
        void request_preemption(SelectReason reason = SelectReason::TICK) noexcept;
        [[noreturn]] void exit_current() noexcept;
        void request_reschedule(CpuId cpu) noexcept;
        void schedule() noexcept;
        void become_idle() noexcept;
        [[noreturn]] void bootstrap_current() noexcept;

        [[nodiscard]] bool ready() const noexcept {
            return ready_;
        }
        [[nodiscard]] task::Thread *current() const noexcept;
        [[nodiscard]] task::Thread &current_thread() const noexcept;
        [[nodiscard]] RunQueue &local_run_queue() noexcept {
            return run_queue_;
        }
        [[nodiscard]] hal::CpuClockDeadline preemption_deadline() noexcept;

    private:
        struct SwitchPair final {
            task::Thread *previous = nullptr;
            task::Thread *next     = nullptr;

            [[nodiscard]] bool required() const noexcept {
                return previous != nullptr && next != nullptr && previous != next;
            }
        };

        [[nodiscard]] units::time now() const noexcept;
        [[nodiscard]] SelectResult select_locked(SelectReason reason, units::time now,
                                                 bool force) noexcept;
        [[nodiscard]] SchedEntity &take_candidate_locked(const SelectResult &selection,
                                                         units::time now) noexcept;
        void publish_current_locked(SchedEntity &entity, units::time now) noexcept;
        [[nodiscard]] SwitchPair switch_runnable_current_locked(EnterReason enter_reason,
                                                                SelectReason select_reason,
                                                                units::time now) noexcept;
        void wake_blocked_locked(task::Thread &thread, units::time now) noexcept;
        void validate_block_token_locked(const BlockToken &token) const noexcept;
        void consume_current_slice_locked(task::Thread &thread, units::time now,
                                          SelectReason reason) noexcept;
        void refresh_preemption_deadline_locked(units::time now) noexcept;
        void publish_preemption_deadline_locked(hal::CpuClockDeadline deadline) noexcept;
        void mark_need_resched_locked(SelectReason reason) noexcept;
        void switch_to(task::Thread &previous, task::Thread &next) noexcept;

        RunQueue run_queue_{CpuId{.value = 0}};
        SchedClassSet class_set_{};
        SchedAccounting accounting_{};
        cap::ObjectRef<task::Thread> deferred_exit_{};
        PreemptionDeadlineSink deadline_sink_{};
        hal::CpuClockDeadline preemption_deadline_ = hal::CpuClockDeadline::disarmed();
        bool ready_                                = false;
    };

    SchedulerCore &instance() noexcept;
    void yield() noexcept;
    [[noreturn]] void exit_current() noexcept;

    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept;
}  // namespace scheduler
