/**
 * @file timer.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 当前 CPU 的架构时钟源与 one-shot timer 接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/namespace.h>
#include <tay/bits.h>
#include <tay/units.h>

#include <atomic>
#include <concepts>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    /** @brief 以架构单调计数器 epoch 为基准的绝对 timer deadline。 */
    struct TimerDeadline final {
        units::time when{};
        bool armed = false;

        [[nodiscard]] static constexpr TimerDeadline at(units::time when) noexcept {
            return TimerDeadline{.when = when, .armed = true};
        }

        [[nodiscard]] static constexpr TimerDeadline disarmed() noexcept {
            return TimerDeadline{};
        }

        [[nodiscard]] constexpr bool operator==(const TimerDeadline &other) const noexcept {
            return armed == other.armed && (!armed || when == other.when);
        }

        [[nodiscard]] constexpr bool operator!=(const TimerDeadline &other) const noexcept {
            return !(*this == other);
        }
    };

    template <class T>
    concept ClockTraits = requires(T &clock, TimerDeadline deadline) {
        {
            T::instance()
        } noexcept -> std::same_as<T &>;
        {
            clock.now()
        } noexcept -> std::same_as<units::time>;
        {
            clock.set_deadline(deadline)
        } noexcept -> std::same_as<void>;
        {
            clock.available()
        } noexcept -> std::same_as<bool>;
        {
            clock.raw_ticks()
        } noexcept -> std::same_as<u64_t>;
    };

    /**
     * @brief 当前架构的 per-CPU 时钟硬件入口。
     * @note 单例保存不可变的全局时基频率，硬件寄存器操作始终作用于当前 CPU。
     */
    class Clock final {
    public:
        [[nodiscard]] static Clock &instance() noexcept;

        /** @brief 兼容入口：发布全局频率并初始化当前 CPU timer。 */
        void initialize(u64_t frequency_hz) noexcept;
        /** @brief BSP-only 发布不可变全局时基频率。 */
        void init_freq(u64_t frequency_hz) noexcept;
        /** @brief 初始化当前 CPU 的本地 timer source。 */
        void initialize_local() noexcept;

        [[nodiscard]] units::time now() const noexcept;
        void set_deadline(TimerDeadline deadline) noexcept;
        [[nodiscard]] bool available() const noexcept;
        [[nodiscard]] u64_t raw_ticks() const noexcept;

        Clock(const Clock &)            = delete;
        Clock &operator=(const Clock &) = delete;
        Clock(Clock &&)                 = delete;
        Clock &operator=(Clock &&)      = delete;

    private:
        constexpr Clock() noexcept = default;

        static Clock instance_;

        std::atomic<u64_t> frequency_hz_{0};
    };

    static_assert(ClockTraits<Clock>);
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
