/**
 * @file early.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V SBI 内核早期控制台后端
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/early_console.h>
#include <arch/riscv64/namespace.h>
#include <sbi/sbi.h>
#include <tay/bits.h>

namespace riscv64::hal {
    constinit EarlyConsole EarlyConsole::instance_;

    EarlyConsole &early_console() noexcept {
        return EarlyConsole::instance_;
    }

    void EarlyConsole::putc(char ch) noexcept {
        (void)sbi_dbcn_console_write_byte(ch);
    }

    [[noreturn]] void EarlyConsole::halt() noexcept {
        while (true) asm volatile("wfi" ::: "memory");
    }
}  // namespace riscv64::hal
