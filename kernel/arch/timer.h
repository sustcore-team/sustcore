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

#include <concepts>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    /** @brief 以架构单调计数器 epoch 为基准的绝对 timer deadline。 */
    struct CpuClockDeadline final {
        units::time when{};
        bool armed = false;

        [[nodiscard]] static constexpr CpuClockDeadline at(units::time when) noexcept {
            return CpuClockDeadline{.when = when, .armed = true};
        }

        [[nodiscard]] static constexpr CpuClockDeadline disarmed() noexcept {
            return CpuClockDeadline{};
        }

        [[nodiscard]] constexpr bool operator==(const CpuClockDeadline &other) const noexcept {
            return armed == other.armed && (!armed || when == other.when);
        }

        [[nodiscard]] constexpr bool operator!=(const CpuClockDeadline &other) const noexcept {
            return !(*this == other);
        }
    };

    template <class T>
    concept CpuClockTraits = requires(T &clock, CpuClockDeadline deadline) {
        {
            T::instance()
        } noexcept -> std::same_as<T &>;
        {
            clock.current_time()
        } noexcept -> std::same_as<units::time>;
        {
            clock.set_timer_deadline(deadline)
        } noexcept -> std::same_as<void>;
        {
            clock.available()
        } noexcept -> std::same_as<bool>;
        {
            clock.raw_timestamp_counter()
        } noexcept -> std::same_as<u64_t>;
    };

    /**
     * @brief 当前架构的 per-CPU 时钟硬件入口。
     * @note 单例保存不可变的全局时基频率，硬件寄存器操作始终作用于当前 CPU。
     */
    class CpuClock final {
    public:
        [[nodiscard]] static CpuClock &instance() noexcept;

        /** @brief 保存平台时基并打开当前 CPU 的本地 timer source。 */
        void initialize(u64_t frequency_hz) noexcept;

        [[nodiscard]] units::time current_time() const noexcept;
        void set_timer_deadline(CpuClockDeadline deadline) noexcept;
        [[nodiscard]] bool available() const noexcept;
        [[nodiscard]] u64_t raw_timestamp_counter() const noexcept;

        CpuClock(const CpuClock &)            = delete;
        CpuClock &operator=(const CpuClock &) = delete;
        CpuClock(CpuClock &&)                 = delete;
        CpuClock &operator=(CpuClock &&)      = delete;

    private:
        constexpr CpuClock() noexcept = default;

        static CpuClock instance_;

        u64_t frequency_hz_ = 0;
    };

    static_assert(CpuClockTraits<CpuClock>);
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
