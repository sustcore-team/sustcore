/**
 * @file clock.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 架构时钟设备
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/clock.h>
#include <arch/riscv64/device/platform.h>
#include <device/model.h>
#include <device/platform.h>
#include <logger.h>
#include <sbi/sbi.h>

namespace riscv {
    units::tick CSRTimeClockSource::now() const noexcept {
        return csr_get_time();
    }

    units::frequency CSRTimeClockSource::frequency() const noexcept {
        return _freq;
    }

    void ClintAlarm::set_next_event(units::time delta) noexcept {
        units::tick now  = _clksrc->now();
        units::tick gaps = delta * _clksrc->frequency();
        sbi_legacy_set_timer(now + gaps);
    }

    ClintAlarm::ClintAlarm(driver::ClockSource *clksrc,
                           driver::virq_t clock_virq) noexcept
        : driver::Alarm(clksrc), _clock_virq(clock_virq) {
        _last_recorded_time = _clksrc->to_ns(_clksrc->now());
        assert(device::DeviceModel::initialized());
        auto *platform = device::DeviceModel::inst().platform();
        assert(platform != nullptr);
        assert(platform->is<Riscv64Platform>());
        auto &irqman      = device::DeviceModel::inst().interrupt();
        auto register_res = irqman.register_handler(
            clock_virq, this_call(this, &ClintAlarm::handle_irq));
        assert(register_res.has_value());
    }

    void ClintAlarm::handle_irq(const driver::IrqEvent &event) noexcept {
        if (event.virq != _clock_virq) {
            loggers::INTERRUPT::ERROR(
                "ClintAlarm 收到不匹配的 virq: got=%llu expect=%llu",
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
            loggers::INTERRUPT::ERROR("ClintAlarm 处理 IRQ 时 ack 失败: %s",
                                      to_cstring(ack_res.error()));
        }
    }
}  // namespace riscv
