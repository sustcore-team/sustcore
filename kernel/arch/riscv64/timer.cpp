/**
 * @file timer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 当前 CPU 的时钟源与 SBI timer 后端。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/csr.h>
#include <arch/riscv64/namespace.h>
#include <arch/timer.h>
#include <log.h>
#include <sbi/sbi.h>

namespace riscv64::hal {
    namespace {
        constexpr u64_t NANOS_PER_SECOND = 1'000'000'000;
        constexpr u64_t MAX_U64          = static_cast<u64_t>(-1);
        constexpr u64_t MAX_FREQUENCY    = MAX_U64 / NANOS_PER_SECOND;
        constexpr xlen_t SIE_STIE        = xlen_t{1} << 5;

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

        void program_timer(u64_t absolute_ticks) noexcept {
            const auto result = sbi_set_timer(absolute_ticks);
            if (result.error != SBI_SUCCESS)
                kernel::log::panic("SBI TIME 的 set_timer 调用失败");
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
        if (frequency_hz == 0 || frequency_hz > MAX_FREQUENCY)
            kernel::log::panic("无效的 RISC-V timebase-frequency: {}", frequency_hz);
        const auto published = frequency_hz_.load(std::memory_order_acquire);
        if (published != 0 && published != frequency_hz)
            kernel::log::panic("RISC-V timebase-frequency 在初始化后发生变化");

        frequency_hz_.store(frequency_hz, std::memory_order_release);
    }

    void Clock::initialize_local() noexcept {
        if (!available())
            kernel::log::panic("RISC-V Clock 本地初始化前未发布时基频率");
        program_timer(MAX_U64);
        (void)csr::set_bits<csr::CSR::SIE>(SIE_STIE);
    }

    units::time Clock::now() const noexcept {
        if (!available())
            kernel::log::panic("RISC-V Clock 尚未初始化");
        return units::time::from_nanoseconds(
            ticks_to_nanos(raw_ticks(), frequency_hz_.load(std::memory_order_acquire)));
    }

    void Clock::set_deadline(TimerDeadline deadline) noexcept {
        if (!deadline.armed) {
            program_timer(MAX_U64);
            return;
        }
        if (!available())
            kernel::log::panic("RISC-V Clock 尚未初始化");
        program_timer(nanos_to_ticks(deadline.when.to_nanoseconds(),
                                     frequency_hz_.load(std::memory_order_acquire)));
    }

    bool Clock::available() const noexcept {
        return frequency_hz_.load(std::memory_order_acquire) != 0;
    }

    u64_t Clock::raw_ticks() const noexcept {
        u64_t value;
        asm volatile("rdtime %0" : "=r"(value));
        return value;
    }
}  // namespace riscv64::hal
