/**
 * @file clock.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 架构时钟设备
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/int/base.h>

namespace la64 {
    class CSRClockSource : public driver::ClockSource {
    public:
        explicit CSRClockSource(units::frequency freq) noexcept
            : _freq(freq) {}

        [[nodiscard]]
        units::tick now() const noexcept override;

        [[nodiscard]]
        units::frequency frequency() const noexcept override;

    private:
        units::frequency _freq;
    };

    class CSRTimer : public driver::Alarm {
    public:
        explicit CSRTimer(driver::ClockSource *clksrc,
                          driver::virq_t clock_virq) noexcept;

        void set_next_event(units::time delta) noexcept override;

        [[nodiscard]]
        units::time max_delta() const noexcept override {
            return UINT64_MAX / _clksrc->frequency();
        }

        void set_handler(Handler &&handler) noexcept override {
            _handler = std::move(handler);
        }

        void handle_irq(const driver::IrqEvent &event) noexcept;

        [[nodiscard]]
        driver::virq_t clock_virq() const noexcept {
            return _clock_virq;
        }

    private:
        [[nodiscard]]
        static units::tick clamp_ticks(units::tick ticks) noexcept;

        Handler _handler;
        units::time _last_recorded_time{};
        driver::virq_t _clock_virq = 0;
    };
}  // namespace la64
