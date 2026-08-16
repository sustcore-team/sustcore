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
#include <scheduler/scheduler.h>
#include <tay/spinlock.h>

namespace kernel::timer {
    [[nodiscard]] constexpr units::time saturated_deadline_after(
        units::time now, units::duration duration) noexcept {
        return units::saturated_add(now, duration);
    }

    class DeadlineState;

    /** @brief 一次硬件 timer IRQ 的线性 session，必须恰好交给 end_interrupt()。 */
    class DeadlineInterrupt final {
    public:
        DeadlineInterrupt(const DeadlineInterrupt &)            = delete;
        DeadlineInterrupt &operator=(const DeadlineInterrupt &) = delete;
        DeadlineInterrupt(DeadlineInterrupt &&other) noexcept;
        DeadlineInterrupt &operator=(DeadlineInterrupt &&other) noexcept;
        ~DeadlineInterrupt() noexcept;

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
        DeadlineInterrupt(DeadlineState &owner, u64_t sequence, units::time now, bool timer_due,
                          bool preemption_due) noexcept
            : owner_(&owner),
              sequence_(sequence),
              now_(now),
              timer_due_(timer_due),
              preemption_due_(preemption_due) {}
        void invalidate() noexcept;

        DeadlineState *owner_ = nullptr;
        u64_t sequence_       = 0;
        units::time now_{};
        bool timer_due_      = false;
        bool preemption_due_ = false;

        friend class DeadlineState;
    };

    /**
     * @brief 当前 BSP 的两源 deadline 合并状态。
     *
     * scheduler/engine 可以在各自锁下向本对象发布，因此固定锁序为
     * scheduler-or-engine -> DeadlineState。IRQ 调用 scheduler 前必须先结束本对象临界区。
     */
    class DeadlineState final {
    public:
        constexpr DeadlineState() noexcept              = default;
        DeadlineState(const DeadlineState &)            = delete;
        DeadlineState &operator=(const DeadlineState &) = delete;

        void initialize(hal::CpuClock &clock) noexcept;
        void publish_timer(hal::CpuClockDeadline deadline) noexcept;
        void publish_preemption(hal::CpuClockDeadline deadline) noexcept;

        [[nodiscard]] DeadlineInterrupt begin_interrupt(units::time now) noexcept;
        void end_interrupt(DeadlineInterrupt &&interrupt) noexcept;

        [[nodiscard]] scheduler::PreemptionDeadlineSink preemption_sink() noexcept;
        [[nodiscard]] hal::CpuClockDeadline timer_deadline() noexcept;
        [[nodiscard]] hal::CpuClockDeadline preemption_deadline() noexcept;
        [[nodiscard]] hal::CpuClockDeadline programmed_deadline() noexcept;

    private:
        static void publish_preemption_from_scheduler(void *context,
                                                      hal::CpuClockDeadline deadline) noexcept;
        [[nodiscard]] hal::CpuClockDeadline merged_locked() const noexcept;
        void program_locked(bool force) noexcept;

        tay::spinlock lock_{};
        hal::CpuClock *clock_                      = nullptr;
        hal::CpuClockDeadline timer_deadline_      = hal::CpuClockDeadline::disarmed();
        hal::CpuClockDeadline preemption_deadline_ = hal::CpuClockDeadline::disarmed();
        hal::CpuClockDeadline programmed_deadline_ = hal::CpuClockDeadline::disarmed();
        u64_t interrupt_sequence_                  = 0;
        bool programmed_valid_                     = false;
        bool interrupt_active_                     = false;
    };

    [[nodiscard]] DeadlineState &bsp_deadline_state() noexcept;
}  // namespace kernel::timer
