/**
 * @file timer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 时钟源与时钟事件适配
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/csr.h>
#include <arch/timer.h>
#include <firmware/fdt/tree.h>
#include <log.h>
#include <sbi/sbi.h>

namespace riscv64::timer {
    u64_t init_timer(const firmware::fdt::Tree &tree) noexcept {
        auto cpus      = tree.path("/cpus");
        auto property  = cpus.property("timebase-frequency");
        auto frequency = property ? property->cell(0)
                                  : tay::expected<u32_t, tay::error_code>(
                                        tay::unexpect, tay::error_code::OUT_OF_RANGE);
        if (!frequency || *frequency == 0)
            kernel::log::panic("缺少 RISC-V timebase-frequency");
        (void)hal::csr::set_bits<hal::csr::CSR::SIE>(xlen_t{1} << 5);
        return *frequency;
    }

    u64_t read_timer_ticks() noexcept {
        u64_t value;
        asm volatile("rdtime %0" : "=r"(value));
        return value;
    }

    void program_timer(u64_t absolute_ticks) noexcept {
        const auto result = sbi_set_timer(absolute_ticks);
        if (result.error != SBI_SUCCESS)
            kernel::log::panic("SBI TIME 的 set_timer 调用失败");
    }

    void cancel_timer() noexcept {
        (void)sbi_set_timer(UINT64_MAX);
    }

    void acknowledge_timer() noexcept {}
}  // namespace riscv64::timer
