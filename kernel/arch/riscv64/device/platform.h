/**
 * @file platform.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 平台设备
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/device/clock.h>
#include <device/platform.h>
#include <sus/owner.h>
#include <sus/units.h>

namespace riscv {
    class Riscv64Platform : public device::Platform {
    public:
        static constexpr device::PlatformType IDENTIFIER =
            device::PlatformType::RISCV64;

        explicit Riscv64Platform(units::frequency timebase_frequency) noexcept
            : _timebase_frequency(timebase_frequency),
              _clock_source(new CSRTimeClockSource(timebase_frequency)) {}

        [[nodiscard]]
        device::PlatformType type_id() const override {
            return IDENTIFIER;
        }

        [[nodiscard]]
        units::frequency timebase_frequency() const noexcept {
            return _timebase_frequency;
        }

        [[nodiscard]]
        driver::ClockSource *clock_source() noexcept override {
            return _clock_source.get();
        }

    private:
        units::frequency _timebase_frequency;
        util::owner<driver::ClockSource *> _clock_source;
    };
}  // namespace riscv
