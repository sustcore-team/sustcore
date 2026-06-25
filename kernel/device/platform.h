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
#include <driver/base.h>
#include <sus/rtti.h>

#include <string>

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

        [[nodiscard]]
        driver::IShutdownDriver *shutdown_driver() noexcept {
            return _shutdown_driver;
        }

        [[nodiscard]]
        const driver::IShutdownDriver *shutdown_driver() const noexcept {
            return _shutdown_driver;
        }

        void set_shutdown_driver(driver::IShutdownDriver *driver) noexcept {
            _shutdown_driver = driver;
        }

        void clear_shutdown_driver(
            const driver::IShutdownDriver *driver) noexcept {
            if (_shutdown_driver == driver) {
                _shutdown_driver = nullptr;
            }
        }

        [[nodiscard]]
        const std::string &stdout_device_dir() const noexcept {
            return _stdout_device_dir;
        }

        void set_stdout_device_dir(std::string path) noexcept {
            _stdout_device_dir = std::move(path);
        }

    private:
        driver::IShutdownDriver *_shutdown_driver = nullptr;
        std::string _stdout_device_dir{};
    };
}  // namespace device
