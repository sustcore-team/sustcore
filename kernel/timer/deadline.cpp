/**
 * @file deadline.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief one-shot deadline 合并、IRQ quiesce 与强制 rearm。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <log.h>
#include <tay/lock.h>
#include <timer/deadline.h>

namespace kernel::timer {
    namespace {
        constinit DeadlineState bsp_state;

        [[nodiscard]] constexpr bool same_deadline(hal::CpuClockDeadline left,
                                                   hal::CpuClockDeadline right) noexcept {
            return left.armed == right.armed && (!left.armed || left.when == right.when);
        }

        [[nodiscard]] constexpr bool due(hal::CpuClockDeadline deadline, units::time now) noexcept {
            return deadline.armed && deadline.when <= now;
        }
    }  // namespace

    static_assert(saturated_deadline_after(units::time::max() - 1_ns, 2_ns) == units::time::max());

    DeadlineInterrupt::DeadlineInterrupt(DeadlineInterrupt &&other) noexcept
        : owner_(other.owner_),
          sequence_(other.sequence_),
          now_(other.now_),
          timer_due_(other.timer_due_),
          preemption_due_(other.preemption_due_) {
        other.invalidate();
    }

    DeadlineInterrupt &DeadlineInterrupt::operator=(DeadlineInterrupt &&other) noexcept {
        if (this == &other)
            return *this;
        if (owner_ != nullptr)
            kernel::log::panic("overwriting an active DeadlineInterrupt");
        owner_          = other.owner_;
        sequence_       = other.sequence_;
        now_            = other.now_;
        timer_due_      = other.timer_due_;
        preemption_due_ = other.preemption_due_;
        other.invalidate();
        return *this;
    }

    DeadlineInterrupt::~DeadlineInterrupt() noexcept {
        if (owner_ != nullptr)
            kernel::log::panic("timer IRQ did not end its DeadlineInterrupt session");
    }

    void DeadlineInterrupt::invalidate() noexcept {
        owner_    = nullptr;
        sequence_ = 0;
    }

    DeadlineState &bsp_deadline_state() noexcept {
        return bsp_state;
    }

    void DeadlineState::initialize(hal::CpuClock &clock) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (clock_ != nullptr || !clock.available())
            kernel::log::panic("invalid DeadlineState initialization");
        clock_               = &clock;
        timer_deadline_      = hal::CpuClockDeadline::disarmed();
        preemption_deadline_ = hal::CpuClockDeadline::disarmed();
        programmed_valid_    = false;
        program_locked(true);
    }

    hal::CpuClockDeadline DeadlineState::merged_locked() const noexcept {
        if (!timer_deadline_.armed)
            return preemption_deadline_;
        if (!preemption_deadline_.armed)
            return timer_deadline_;
        return timer_deadline_.when <= preemption_deadline_.when ? timer_deadline_
                                                                 : preemption_deadline_;
    }

    void DeadlineState::program_locked(bool force) noexcept {
        if (clock_ == nullptr)
            kernel::log::panic("programming an uninitialized DeadlineState");
        const auto merged = merged_locked();
        if (!force && programmed_valid_ && same_deadline(programmed_deadline_, merged))
            return;
        clock_->set_timer_deadline(merged);
        programmed_deadline_ = merged;
        programmed_valid_    = true;
    }

    void DeadlineState::publish_timer(hal::CpuClockDeadline deadline) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (clock_ == nullptr)
            kernel::log::panic("publishing timer root before DeadlineState initialization");
        timer_deadline_ = deadline;
        if (!interrupt_active_)
            program_locked(false);
    }

    void DeadlineState::publish_preemption(hal::CpuClockDeadline deadline) noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (clock_ == nullptr)
            kernel::log::panic("publishing scheduler deadline before DeadlineState initialization");
        preemption_deadline_ = deadline;
        if (!interrupt_active_)
            program_locked(false);
    }

    DeadlineInterrupt DeadlineState::begin_interrupt(units::time now) noexcept {
        if (hal::interrupts_enabled())
            kernel::log::panic("timer IRQ begin requires local interrupts disabled");
        tay::lock_guard guard(lock_);
        if (clock_ == nullptr || interrupt_active_)
            kernel::log::panic("invalid or nested timer IRQ deadline session");

        // 架构层没有独立 ack ABI。先强制 quiesce，end_interrupt() 再依据最新 publication rearm。
        clock_->set_timer_deadline(hal::CpuClockDeadline::disarmed());
        programmed_valid_ = false;
        interrupt_active_ = true;
        ++interrupt_sequence_;
        if (interrupt_sequence_ == 0)
            ++interrupt_sequence_;
        return DeadlineInterrupt(*this, interrupt_sequence_, now, due(timer_deadline_, now),
                                 due(preemption_deadline_, now));
    }

    void DeadlineState::end_interrupt(DeadlineInterrupt &&interrupt) noexcept {
        if (hal::interrupts_enabled())
            kernel::log::panic("timer IRQ end requires local interrupts disabled");
        tay::lock_guard guard(lock_);
        if (interrupt.owner_ != this || interrupt.sequence_ != interrupt_sequence_ ||
            !interrupt_active_)
            kernel::log::panic("mismatched timer IRQ deadline session");
        interrupt.invalidate();
        interrupt_active_ = false;
        program_locked(true);
    }

    void DeadlineState::publish_preemption_from_scheduler(void *context,
                                                          hal::CpuClockDeadline deadline) noexcept {
        auto *state = static_cast<DeadlineState *>(context);
        if (state == nullptr)
            kernel::log::panic("scheduler installed a null DeadlineState sink");
        state->publish_preemption(deadline);
    }

    scheduler::PreemptionDeadlineSink DeadlineState::preemption_sink() noexcept {
        return scheduler::PreemptionDeadlineSink{
            .context = this,
            .publish = publish_preemption_from_scheduler,
        };
    }

    hal::CpuClockDeadline DeadlineState::timer_deadline() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return timer_deadline_;
    }

    hal::CpuClockDeadline DeadlineState::preemption_deadline() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return preemption_deadline_;
    }

    hal::CpuClockDeadline DeadlineState::programmed_deadline() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return programmed_valid_ ? programmed_deadline_ : hal::CpuClockDeadline::disarmed();
    }
}  // namespace kernel::timer
