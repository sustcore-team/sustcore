/**
 * @file scheduler.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Thread 生命周期事件到 enter/leave/select 的单 CPU SchedCore。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/timer.h>
#include <error/sched.h>
#include <obj/thread.h>
#include <scheduler/accounting.h>
#include <scheduler/rq.h>
#include <tay/err.h>
#include <tay/expected.h>

namespace scheduler {
    enum class PlacementKind : u8_t { ANY, PINNED };

    struct Placement final {
        PlacementKind kind = PlacementKind::ANY;
        cpu::CpuId cpu{};

        [[nodiscard]] static constexpr Placement Any() noexcept {
            return Placement{};
        }
        [[nodiscard]] static constexpr Placement Pinned(cpu::CpuId id) noexcept {
            return Placement{.kind = PlacementKind::PINNED, .cpu = id};
        }
    };

    /**
     * @brief 单个 SchedCore 的只读诊断快照。
     *
     * 该结构只描述当前 BSP 队列，不暗示已经存在 per-CPU 调度器。快照由
     * debug_state() 在队列锁内复制，调用者可以在释放锁后安全格式化或记录。
     */
    struct SchedDebug final {
        CpuId cpu{};
        bool ready                 = false;
        bool has_current           = false;
        bool idle_installed        = false;
        bool need_reschedule       = false;
        bool deadline_dirty        = false;
        size_t queued_count        = 0;
        u64_t transition_gen       = 0;
        u64_t enqueue_count        = 0;
        u64_t wake_count           = 0;
        u64_t block_count          = 0;
        u64_t yield_count          = 0;
        u64_t preemption_count     = 0;
        u64_t context_switch_count = 0;
        u64_t exit_count           = 0;
    };

    /**
     * @brief Scheduler 向所属 CPU deadline coordinator 发布唯一的 RR deadline 来源。
     * @note 回调在本地 IRQ 关闭且持 run queue lock 时执行，必须有固定生命周期、不可阻塞、
     * 不可反向调用 scheduler；远端持有目标队列锁时只能置 DEADLINE_DIRTY，不能调用它。
     */
    struct PreemptSink final {
        void *context                                                        = nullptr;
        void (*publish)(void *context, hal::TimerDeadline deadline) noexcept = nullptr;
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

        friend class SchedCore;
    };

    enum class DetachReason : u8_t {
        SUSPEND,
        TERMINATE,
    };

    enum class WakeReason : u8_t {
        EXPLICIT,
        WAIT_COMPLETED,
    };

    class SchedCore final {
    public:
        constexpr SchedCore() noexcept = default;

        /**
         * @brief 绑定固定逻辑 CPU。
         * @pre 仅对应 CPU 的 bring-up owner 可以在 initialize() 前调用。
         */
        void bind_cpu(cpu::CpuId cpu) noexcept;

        SchedCore(const SchedCore &)            = delete;
        SchedCore &operator=(const SchedCore &) = delete;
        SchedCore(SchedCore &&)                 = delete;
        SchedCore &operator=(SchedCore &&)      = delete;

        [[nodiscard]] tay::expected<void, SchedulerError> initialize(
            task::Thread &bootstrap) noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> attach(task::Thread &thread) noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> detach(
            task::Thread &thread, DetachReason reason = DetachReason::SUSPEND) noexcept;
        void wake(task::Thread &thread, WakeReason reason = WakeReason::EXPLICIT) noexcept;
        void notify_work(task::Thread &thread) noexcept;
        [[nodiscard]] tay::expected<BlockToken, SchedulerError> prepare_block() noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> commit_block(BlockToken &&token) noexcept;
        void cancel_block(BlockToken &&token) noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> block() noexcept;

        void yield() noexcept;
        [[nodiscard]] tay::expected<void, SchedulerError> set_preempt_sink(
            PreemptSink sink) noexcept;
        void request_preempt(SelectReason reason = SelectReason::TICK) noexcept;
        [[noreturn]] void exit() noexcept;
        /**
         * @brief 消费目标 RESCHEDULE IPI 并在当前 CPU 刷新其 one-shot deadline。
         * @pre 只能由 run_queue_ 所属 CPU 的 IPI/trap 路径调用。
         */
        void request_reschedule(CpuId cpu) noexcept;
        void schedule() noexcept;
        void become_idle() noexcept;
        [[noreturn]] void bootstrap() noexcept;

        [[nodiscard]] bool ready() const noexcept {
            return ready_;
        }
        [[nodiscard]] task::Thread *current() const noexcept;
        [[nodiscard]] task::Thread &current_thread() const noexcept;
        [[nodiscard]] RunQueue &runq() noexcept {
            return run_queue_;
        }
        [[nodiscard]] hal::TimerDeadline preempt_deadline() noexcept;
        [[nodiscard]] SchedDebug debug_state() noexcept;

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
        [[nodiscard]] SchedEntity &take_next_locked(const SelectResult &selection,
                                                    units::time now) noexcept;
        void set_state_locked(task::Thread &thread, task::ThreadState state) noexcept;
        void set_current_locked(SchedEntity &entity, units::time now) noexcept;
        [[nodiscard]] SwitchPair switch_locked(EnterReason enter_reason, SelectReason select_reason,
                                               units::time now) noexcept;
        void wake_locked(task::Thread &thread, units::time now) noexcept;
        void check_block_locked(const BlockToken &token) const noexcept;
        void charge_rr_locked(task::Thread &thread, units::time now,
                                 SelectReason reason) noexcept;
        void refresh_preempt_locked(units::time now) noexcept;
        void publish_preempt_locked(hal::TimerDeadline deadline) noexcept;
        void flush_preempt_locked() noexcept;
        void set_resched_locked(SelectReason reason) noexcept;
        void switch_to(task::Thread &previous, task::Thread &next) noexcept;

        RunQueue run_queue_{CpuId{.value = 0}};
        SchedClassSet class_set_{};
        SchedAccounting accounting_{};
        cap::KObjectRef<task::Thread> deferred_exit_{};
        PreemptSink deadline_sink_{};
        hal::TimerDeadline preemption_deadline_ = hal::TimerDeadline::disarmed();
        // 记录所有经由 class_set_.enter() 的 ready-queue 入队。
        u64_t enqueue_count_                    = 0;
        u64_t wake_count_                       = 0;
        u64_t block_count_                      = 0;
        u64_t yield_count_                      = 0;
        u64_t preemption_count_                 = 0;
        u64_t context_switch_count_             = 0;
        u64_t exit_count_                       = 0;
        bool ready_                             = false;
    };

    [[nodiscard]] SchedCore &local() noexcept;
    [[nodiscard]] SchedCore &for_cpu(cpu::CpuId cpu) noexcept;
    /**
     * @brief 返回尚未发布到 online 集合的本地 bring-up SchedCore。
     * @pre 仅目标 CPU 自身在中断关闭状态下调用；成功 initialize() 前不得被普通调度路径访问。
     */
    [[nodiscard]] SchedCore &prepare_cpu(cpu::CpuId cpu) noexcept;
    [[nodiscard]] task::Thread *current() noexcept;
    [[nodiscard]] tay::expected<void, SchedulerError> attach(
        task::Thread &thread, Placement placement = Placement::Any()) noexcept;
    void wake(task::Thread &thread, WakeReason reason = WakeReason::EXPLICIT) noexcept;
    /** @brief 按不可变 scheduler owner 转发 WorkQueue 的幂等 runnable 通知。 */
    void notify_work(task::Thread &thread) noexcept;
    void yield() noexcept;
    [[noreturn]] void exit() noexcept;

    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept;
}  // namespace scheduler
