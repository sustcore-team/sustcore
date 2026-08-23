/**
 * @file deadline.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief one-shot deadline 合并、IRQ quiesce 与强制 rearm。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <cpu/local.h>
#include <log.h>
#include <timer/deadline.h>

namespace kernel::timer {
    namespace {
        alignas(64) constinit DeadlineMux states[cpu::MAX_CPUS];

        [[nodiscard]] constexpr bool due(hal::TimerDeadline deadline, units::time now) noexcept {
            return deadline.armed && deadline.when <= now;
        }
    }  // namespace

    static_assert(deadline_after(units::time::max() - 1_ns, 2_ns) == units::time::max());

    DeadlineIrq::DeadlineIrq(DeadlineIrq &&other) noexcept
        : owner_(other.owner_),
          sequence_(other.sequence_),
          now_(other.now_),
          timer_due_(other.timer_due_),
          preemption_due_(other.preemption_due_) {
        other.invalidate();
    }

    DeadlineIrq &DeadlineIrq::operator=(DeadlineIrq &&other) noexcept {
        if (this == &other)
            return *this;
        if (owner_ != nullptr)
            kernel::log::panic("overwriting an active DeadlineIrq");
        owner_          = other.owner_;
        sequence_       = other.sequence_;
        now_            = other.now_;
        timer_due_      = other.timer_due_;
        preemption_due_ = other.preemption_due_;
        other.invalidate();
        return *this;
    }

    DeadlineIrq::~DeadlineIrq() noexcept {
        if (owner_ != nullptr)
            kernel::log::panic("timer IRQ did not end its DeadlineIrq session");
    }

    void DeadlineIrq::invalidate() noexcept {
        owner_    = nullptr;
        sequence_ = 0;
    }

    DeadlineMux &bsp_deadline_mux() noexcept {
        return states[0];
    }

    DeadlineMux &init_deadline_mux(cpu::CpuId cpu, hal::Clock &clock) noexcept {
        if (!cpu.valid())
            kernel::log::panic("invalid deadline CPU id: {}", cpu.value);
        if (cpu != ::cpu::current_id())
            kernel::log::panic("CPU {} attempted to initialize deadline state for CPU {}",
                               ::cpu::current_id().value, cpu.value);
        auto &state = states[cpu.value];
        state.initialize(clock);
        return state;
    }

    DeadlineMux &deadline_mux(cpu::CpuId cpu) noexcept {
        if (!cpu.valid())
            kernel::log::panic("invalid deadline CPU id: {}", cpu.value);
        auto &state = states[cpu.value];
        if (!state.initialized())
            kernel::log::panic("deadline state for CPU {} is not initialized", cpu.value);
        return state;
    }

    DeadlineMux &local_deadline_mux() noexcept {
        return deadline_mux(cpu::current_id());
    }

    void DeadlineMux::initialize(hal::Clock &clock) noexcept {
        auto state = state_.lock();
        if (state->clock != nullptr || state->owner_cpu.value != cpu::INVALID_CPU ||
            !clock.available())
            kernel::log::panic("invalid DeadlineMux initialization");
        state->clock            = &clock;
        state->owner_cpu        = cpu::current_id();
        state->timer_deadline   = hal::TimerDeadline::disarmed();
        state->preempt_deadline = hal::TimerDeadline::disarmed();
        state->programmed_valid = false;
        program_locked(*state, true);
    }

    hal::TimerDeadline DeadlineMux::merged_locked(const State &state) noexcept {
        if (!state.timer_deadline.armed)
            return state.preempt_deadline;
        if (!state.preempt_deadline.armed)
            return state.timer_deadline;
        return state.timer_deadline.when <= state.preempt_deadline.when ? state.timer_deadline
                                                                        : state.preempt_deadline;
    }

    void DeadlineMux::program_locked(State &state, bool force) noexcept {
        if (state.clock == nullptr)
            kernel::log::panic("programming an uninitialized DeadlineMux");
        if (state.owner_cpu != cpu::current_id())
            kernel::log::panic("attempted to program deadline state of CPU {} from CPU {}",
                               state.owner_cpu.value, cpu::current_id().value);
        const auto merged = merged_locked(state);
        if (!force && state.programmed_valid && state.armed_deadline == merged)
            return;
        state.clock->set_deadline(merged);
        state.armed_deadline   = merged;
        state.programmed_valid = true;
    }

    void DeadlineMux::publish_timer(hal::TimerDeadline deadline) noexcept {
        auto state = state_.lock();
        if (state->clock == nullptr)
            kernel::log::panic("publishing timer root before DeadlineMux initialization");
        state->timer_deadline = deadline;
        if (!state->interrupt_active)
            program_locked(*state, false);
    }

    void DeadlineMux::publish_preempt(hal::TimerDeadline deadline) noexcept {
        auto state = state_.lock();
        if (state->clock == nullptr)
            kernel::log::panic("publishing scheduler deadline before DeadlineMux initialization");
        state->preempt_deadline = deadline;
        if (!state->interrupt_active)
            program_locked(*state, false);
    }

    DeadlineIrq DeadlineMux::begin_interrupt(units::time now) noexcept {
        if (hal::irq_enabled())
            kernel::log::panic("timer IRQ begin requires local interrupts disabled");
        auto state = state_.lock();
        if (state->clock == nullptr || state->interrupt_active)
            kernel::log::panic("invalid or nested timer IRQ deadline session");
        if (state->owner_cpu != cpu::current_id())
            kernel::log::panic("timer IRQ for CPU {} entered deadline state of CPU {}",
                               cpu::current_id().value, state->owner_cpu.value);

        // 架构层没有独立 ack ABI。先强制 quiesce，end_interrupt() 再依据最新 publication rearm。
        state->clock->set_deadline(hal::TimerDeadline::disarmed());
        state->programmed_valid = false;
        state->interrupt_active = true;
        ++state->interrupt_sequence;
        if (state->interrupt_sequence == 0)
            ++state->interrupt_sequence;
        return DeadlineIrq(*this, state->interrupt_sequence, now, due(state->timer_deadline, now),
                           due(state->preempt_deadline, now));
    }

    void DeadlineMux::end_interrupt(DeadlineIrq &&interrupt) noexcept {
        if (hal::irq_enabled())
            kernel::log::panic("timer IRQ end requires local interrupts disabled");
        auto state = state_.lock();
        if (interrupt.owner_ != this || interrupt.sequence_ != state->interrupt_sequence ||
            !state->interrupt_active)
            kernel::log::panic("mismatched timer IRQ deadline session");
        interrupt.invalidate();
        state->interrupt_active = false;
        program_locked(*state, true);
    }

    void DeadlineMux::publish_sched(void *context, hal::TimerDeadline deadline) noexcept {
        auto *state = static_cast<DeadlineMux *>(context);
        if (state == nullptr)
            kernel::log::panic("scheduler installed a null DeadlineMux sink");
        state->publish_preempt(deadline);
    }

    scheduler::PreemptSink DeadlineMux::preemption_sink() noexcept {
        return scheduler::PreemptSink{
            .context = this,
            .publish = publish_sched,
        };
    }

    hal::TimerDeadline DeadlineMux::timer_deadline() noexcept {
        auto state = state_.lock();
        return state->timer_deadline;
    }

    hal::TimerDeadline DeadlineMux::preempt_deadline() noexcept {
        auto state = state_.lock();
        return state->preempt_deadline;
    }

    hal::TimerDeadline DeadlineMux::armed_deadline() noexcept {
        auto state = state_.lock();
        return state->programmed_valid ? state->armed_deadline : hal::TimerDeadline::disarmed();
    }

    bool DeadlineMux::initialized() const noexcept {
        auto state = state_.lock();
        return state->clock != nullptr;
    }
}  // namespace kernel::timer
