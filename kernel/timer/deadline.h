/**
 * @file deadline.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief BSP 普通 timer 与 scheduler 抢占 deadline 的 one-shot 合并器。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/timer.h>
#include <cpu/local.h>
#include <scheduler/scheduler.h>
#include <synchronized.h>

namespace kernel::timer {
    [[nodiscard]] constexpr units::time deadline_after(units::time now,
                                                       units::duration duration) noexcept {
        return units::saturated_add(now, duration);
    }

    class DeadlineMux;

    /** @brief 一次硬件 timer IRQ 的线性 session，必须恰好交给 end_interrupt()。 */
    class DeadlineIrq final {
    public:
        DeadlineIrq(const DeadlineIrq &)            = delete;
        DeadlineIrq &operator=(const DeadlineIrq &) = delete;
        DeadlineIrq(DeadlineIrq &&other) noexcept;
        DeadlineIrq &operator=(DeadlineIrq &&other) noexcept;
        ~DeadlineIrq() noexcept;

        [[nodiscard]] units::time now() const noexcept {
            return now_;
        }
        [[nodiscard]] bool timer_due() const noexcept {
            return timer_due_;
        }
        [[nodiscard]] bool preemption_due() const noexcept {
            return preemption_due_;
        }

    private:
        DeadlineIrq(DeadlineMux &owner, u64_t sequence, units::time now, bool timer_due,
                    bool preemption_due) noexcept
            : owner_(&owner),
              sequence_(sequence),
              now_(now),
              timer_due_(timer_due),
              preemption_due_(preemption_due) {}
        void invalidate() noexcept;

        DeadlineMux *owner_ = nullptr;
        u64_t sequence_     = 0;
        units::time now_{};
        bool timer_due_      = false;
        bool preemption_due_ = false;

        friend class DeadlineMux;
    };

    /**
     * @brief 当前 CPU 的两源 deadline 合并状态。
     *
     * scheduler/engine 可以在各自锁下向本对象发布，因此固定锁序为
     * scheduler-or-engine -> DeadlineMux。AP 不使用 timer root；IRQ 调用 scheduler 前必须先
     * 结束本对象临界区。
     */
    class DeadlineMux final {
    public:
        constexpr DeadlineMux() noexcept            = default;
        DeadlineMux(const DeadlineMux &)            = delete;
        DeadlineMux &operator=(const DeadlineMux &) = delete;

        void initialize(hal::Clock &clock) noexcept;
        void publish_timer(hal::TimerDeadline deadline) noexcept;
        void publish_preempt(hal::TimerDeadline deadline) noexcept;

        [[nodiscard]] DeadlineIrq begin_interrupt(units::time now) noexcept;
        void end_interrupt(DeadlineIrq &&interrupt) noexcept;

        [[nodiscard]] scheduler::PreemptSink preemption_sink() noexcept;
        [[nodiscard]] hal::TimerDeadline timer_deadline() noexcept;
        [[nodiscard]] hal::TimerDeadline preempt_deadline() noexcept;
        [[nodiscard]] hal::TimerDeadline armed_deadline() noexcept;
        [[nodiscard]] bool initialized() const noexcept;

    private:
        struct State final {
            hal::Clock *clock = nullptr;
            cpu::CpuId owner_cpu{cpu::INVALID_CPU};
            hal::TimerDeadline timer_deadline   = hal::TimerDeadline::disarmed();
            hal::TimerDeadline preempt_deadline = hal::TimerDeadline::disarmed();
            hal::TimerDeadline armed_deadline   = hal::TimerDeadline::disarmed();
            u64_t interrupt_sequence            = 0;
            bool programmed_valid               = false;
            bool interrupt_active               = false;
        };

        static void publish_sched(void *context, hal::TimerDeadline deadline) noexcept;
        [[nodiscard]] static hal::TimerDeadline merged_locked(const State &state) noexcept;
        static void program_locked(State &state, bool force) noexcept;

        kernel::irq_simple_synchronized<State> state_{};
    };

    [[nodiscard]] DeadlineMux &bsp_deadline_mux() noexcept;
    /** @brief 仅供对应 AP 在 scheduler 发布前初始化其固定 deadline storage。 */
    [[nodiscard]] DeadlineMux &init_deadline_mux(cpu::CpuId cpu, hal::Clock &clock) noexcept;
    [[nodiscard]] DeadlineMux &local_deadline_mux() noexcept;
    [[nodiscard]] DeadlineMux &deadline_mux(cpu::CpuId cpu) noexcept;
}  // namespace kernel::timer
