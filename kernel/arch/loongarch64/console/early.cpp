/**
 * @file early.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 内核早期控制台后端
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/early_console.h>
#include <arch/loongarch64/namespace.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>

#include <cstddef>

namespace loongarch64::hal {
    // LoongArch virt machine NS16550 UART base from QEMU virt platform.
    constexpr addr_t UART_PADDR       = 0x1FE001E0;
    constexpr size_t TRANSMIT_HOLDING = 0;
    constexpr size_t LINE_STATUS      = 5;
    constexpr u8_t TRANSMIT_EMPTY     = 0x20;

    constinit EarlyConsole EarlyConsole::instance_;

    EarlyConsole &early_console() noexcept {
        return EarlyConsole::instance_;
    }

    void EarlyConsole::putc(char ch) noexcept {
        auto *serial = reinterpret_cast<volatile u8_t *>(KPA_START + UART_PADDR);
        while ((serial[LINE_STATUS] & TRANSMIT_EMPTY) == 0) {
        }
        serial[TRANSMIT_HOLDING] = static_cast<u8_t>(ch);
    }

    [[noreturn]] void EarlyConsole::halt() noexcept {
        while (true) asm volatile("idle 0" ::: "memory");
    }
}  // namespace loongarch64::hal
