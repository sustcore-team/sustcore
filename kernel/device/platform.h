/**
 * @file platform.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 平台设备
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/clock.h>
#include <sus/rtti.h>

namespace device {
    enum class PlatformType {
        RISCV64,
        LOONGARCH64
    };

    class Platform : public RTTIBase<Platform, PlatformType> {
    public:
        virtual ~Platform() = default;

        [[nodiscard]]
        virtual driver::ClockSource *clock_source() noexcept = 0;
    };
}  // namespace device
