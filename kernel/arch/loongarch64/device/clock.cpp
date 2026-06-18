/**
 * @file clock.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 架构时钟设备
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/csr.h>
#include <arch/loongarch64/device/clock.h>
#include <device/model.h>
#include <logger.h>

namespace la64 {
    namespace {
        [[nodiscard]]
        units::tick read_time_counter() noexcept {
            uint64_t counter = 0;
            uint64_t tid     = 0;
            asm volatile("rdtime.d %0, %1" : "=r"(counter), "=r"(tid));
            (void)tid;
            return counter;
        }
    }  // namespace

    units::tick CSRClockSource::now() const noexcept {
        return read_time_counter();
    }

    units::frequency CSRClockSource::frequency() const noexcept {
        return _freq;
    }

    units::tick CSRTimer::clamp_ticks(units::tick ticks) noexcept {
        if (ticks == 0) {
            return 1;
        }
        constexpr units::tick MAX_INITVAL = (1ULL << 30) - 1;
        return ticks > MAX_INITVAL ? MAX_INITVAL : ticks;
    }

    CSRTimer::CSRTimer(driver::ClockSource *clksrc,
                       driver::virq_t clock_virq) noexcept
        : driver::Alarm(clksrc), _clock_virq(clock_virq) {
        _last_recorded_time = _clksrc->to_ns(_clksrc->now());
        auto &irqman        = device::DeviceModel::inst().interrupt();
        auto register_res = irqman.register_handler(
            clock_virq, this_call(this, &CSRTimer::handle_irq));
        assert(register_res.has_value());
    }

    void CSRTimer::set_next_event(units::time delta) noexcept {
        units::tick ticks = clamp_ticks(delta * _clksrc->frequency());

        csr_ticlr_t ticlr{};
        ticlr.clr = 1;
        csr_set_ticlr(ticlr);

        csr_tcfg_t tcfg{};
        tcfg.en       = 1;
        tcfg.periodic = 0;
        tcfg.initval  = static_cast<uint32_t>(ticks);
        csr_set_tcfg(tcfg);
    }

    void CSRTimer::handle_irq(const driver::IrqEvent &event) noexcept {
        if (event.virq != _clock_virq) {
            loggers::INTERRUPT::ERROR(
                "CSRTimer 收到不匹配的 virq: got=%llu expect=%llu",
                static_cast<unsigned long long>(event.virq),
                static_cast<unsigned long long>(_clock_virq));
            return;
        }

        units::time now = _clksrc->to_ns(_clksrc->now());
        if (_handler) {
            _handler(driver::ClockEvent{.last = _last_recorded_time, .now = now});
        }
        _last_recorded_time = now;

        auto ack_res = device::DeviceModel::inst().interrupt().ack(event);
        if (!ack_res.has_value()) {
            loggers::INTERRUPT::ERROR("CSRTimer 处理 IRQ 时 ack 失败: %s",
                                      to_cstring(ack_res.error()));
        }
    }
}  // namespace la64
