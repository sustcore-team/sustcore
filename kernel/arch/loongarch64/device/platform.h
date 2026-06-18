/**
 * @file platform.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 平台设备
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/device/clock.h>
#include <device/platform.h>
#include <device/device.h>
#include <sus/owner.h>
#include <sus/units.h>

namespace la64 {
    class LoongArch64Platform : public device::Platform {
    public:
        static constexpr device::PlatformType IDENTIFIER =
            device::PlatformType::LOONGARCH64;

        explicit LoongArch64Platform(units::frequency timer_frequency) noexcept
            : _timer_frequency(timer_frequency),
              _clock_source(new CSRClockSource(timer_frequency)) {}

        [[nodiscard]]
        device::PlatformType type_id() const override {
            return IDENTIFIER;
        }

        [[nodiscard]]
        units::frequency timer_frequency() const noexcept {
            return _timer_frequency;
        }

        [[nodiscard]]
        driver::ClockSource *clock_source() noexcept override {
            return _clock_source.get();
        }

        [[nodiscard]]
        driver::intc_t global_intc() const noexcept {
            return _global_intc;
        }

        void set_global_intc(driver::intc_t global_intc) noexcept {
            _global_intc = global_intc;
        }

    private:
        units::frequency _timer_frequency;
        util::owner<driver::ClockSource *> _clock_source;
        driver::intc_t _global_intc = device::INVALID_ICTRL_ID;
    };
}  // namespace la64
