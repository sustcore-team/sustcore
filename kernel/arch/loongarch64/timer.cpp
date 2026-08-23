/**
 * @file timer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 当前 CPU 的恒定计数器与 one-shot timer 后端。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/csr.h>
#include <arch/loongarch64/namespace.h>
#include <arch/loongarch64/valdef.h>
#include <arch/timer.h>
#include <log.h>

namespace loongarch64::hal {
    namespace {
        constexpr u64_t NANOS_PER_SECOND   = 1'000'000'000;
        constexpr u64_t MAX_U64            = static_cast<u64_t>(-1);
        constexpr u64_t MAX_FREQUENCY      = MAX_U64 / NANOS_PER_SECOND;
        constexpr u64_t MAX_INITVAL        = (u64_t{1} << 30) - 1;
        constexpr xlen_t CPUCFG_CCFREQ     = 0x4;
        constexpr xlen_t CPUCFG_CCSCALE    = 0x5;
        constexpr u32_t CPUCFG_CCMUL_MASK  = 0x0000ffff;
        constexpr u32_t CPUCFG_CCDIV_MASK  = 0xffff0000;
        constexpr u32_t CPUCFG_CCDIV_SHIFT = 16;

        [[nodiscard]] u32_t read_cpucfg(xlen_t index) noexcept {
            xlen_t value;
            asm volatile("cpucfg %0, %1" : "=r"(value) : "r"(index));
            return static_cast<u32_t>(value);
        }

        [[nodiscard]] u64_t counter_frequency() noexcept {
            const u64_t base       = read_cpucfg(CPUCFG_CCFREQ);
            const u32_t scale      = read_cpucfg(CPUCFG_CCSCALE);
            const u64_t multiplier = scale & CPUCFG_CCMUL_MASK;
            const u64_t divisor    = (scale & CPUCFG_CCDIV_MASK) >> CPUCFG_CCDIV_SHIFT;
            if (base == 0 || multiplier == 0 || divisor == 0)
                return 0;
            return base * multiplier / divisor;
        }

        [[nodiscard]] u64_t ticks_to_nanos(u64_t ticks, u64_t frequency_hz) noexcept {
            const u64_t seconds   = ticks / frequency_hz;
            const u64_t remainder = ticks % frequency_hz;
            if (seconds > MAX_U64 / NANOS_PER_SECOND)
                return MAX_U64;
            return seconds * NANOS_PER_SECOND + remainder * NANOS_PER_SECOND / frequency_hz;
        }

        [[nodiscard]] u64_t nanos_to_ticks(u64_t nanos, u64_t frequency_hz) noexcept {
            const u64_t seconds   = nanos / NANOS_PER_SECOND;
            const u64_t remainder = nanos % NANOS_PER_SECOND;
            if (seconds > MAX_U64 / frequency_hz)
                return MAX_U64;

            const u64_t product  = remainder * frequency_hz;
            u64_t partial_ticks  = product / NANOS_PER_SECOND;
            partial_ticks       += product % NANOS_PER_SECOND == 0 ? 0 : 1;
            const u64_t ticks    = seconds * frequency_hz;
            return partial_ticks > MAX_U64 - ticks ? MAX_U64 : ticks + partial_ticks;
        }

        [[nodiscard]] u64_t clamp_initval(u64_t ticks) noexcept {
            if (ticks == 0)
                return 1;
            return ticks > MAX_INITVAL ? MAX_INITVAL : ticks;
        }

        void disarm_timer() noexcept {
            csr::write<csr::CSR::TCFG>(0);
            csr::write<csr::CSR::TICLR>(TICLR_CLR);
        }
    }  // namespace

    constinit Clock Clock::instance_;

    Clock &Clock::instance() noexcept {
        return instance_;
    }

    void Clock::initialize(u64_t frequency_hz) noexcept {
        init_freq(frequency_hz);
        initialize_local();
    }

    void Clock::init_freq(u64_t frequency_hz) noexcept {
        const u64_t hardware_frequency = counter_frequency();
        if (frequency_hz == 0)
            frequency_hz = hardware_frequency;
        else if (hardware_frequency != 0 && hardware_frequency != frequency_hz)
            kernel::log::panic("LoongArch 目录与 CPUCFG 的时基频率不一致: catalog={}, cpucfg={}",
                               frequency_hz, hardware_frequency);
        if (frequency_hz == 0 || frequency_hz > MAX_FREQUENCY)
            kernel::log::panic("无效的 LoongArch timebase-frequency: {}", frequency_hz);
        const auto published = frequency_hz_.load(std::memory_order_acquire);
        if (published != 0 && published != frequency_hz)
            kernel::log::panic("LoongArch timebase-frequency 在初始化后发生变化");

        frequency_hz_.store(frequency_hz, std::memory_order_release);
    }

    void Clock::initialize_local() noexcept {
        if (!available())
            kernel::log::panic("LoongArch Clock 本地初始化前未发布时基频率");
        disarm_timer();
        (void)csr::set_bits<csr::CSR::ECFG>(ECFG_TIMER);
    }

    units::time Clock::now() const noexcept {
        if (!available())
            kernel::log::panic("LoongArch Clock 尚未初始化");
        return units::time::from_nanoseconds(
            ticks_to_nanos(raw_ticks(), frequency_hz_.load(std::memory_order_acquire)));
    }

    void Clock::set_deadline(TimerDeadline deadline) noexcept {
        if (!deadline.armed) {
            disarm_timer();
            return;
        }
        if (!available())
            kernel::log::panic("LoongArch Clock 尚未初始化");

        const u64_t now_ns         = now().to_nanoseconds();
        const u64_t deadline_nanos = deadline.when.to_nanoseconds();
        const u64_t delta          = deadline_nanos > now_ns ? deadline_nanos - now_ns : 0;
        const u64_t ticks =
            clamp_initval(nanos_to_ticks(delta, frequency_hz_.load(std::memory_order_acquire)));
        csr::write<csr::CSR::TICLR>(TICLR_CLR);
        csr::write<csr::CSR::TCFG>((ticks << TCFG_INITVAL_SHIFT) | TCFG_EN);
    }

    bool Clock::available() const noexcept {
        return frequency_hz_.load(std::memory_order_acquire) != 0;
    }

    u64_t Clock::raw_ticks() const noexcept {
        u64_t counter;
        u64_t timer_id;
        asm volatile("rdtime.d %0, %1" : "=r"(counter), "=r"(timer_id));
        (void)timer_id;
        return counter;
    }
}  // namespace loongarch64::hal
