/**
 * @file csr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 控制状态寄存器访问
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>
#include <tay/bits.h>

namespace riscv64::hal::csr {
    enum class CSR : u16_t {
        SSTATUS  = 0x100,
        SIE      = 0x104,
        STVEC    = 0x105,
        SSCRATCH = 0x140,
        SEPC     = 0x141,
        SCAUSE   = 0x142,
        SIP      = 0x144,
        STVAL    = 0x143,
        SATP     = 0x180,
    };

    template <CSR Register>
    [[nodiscard]] inline xlen_t read() noexcept {
        xlen_t value;
        asm volatile("csrr %0, %1" : "=r"(value) : "i"(static_cast<u16_t>(Register)));
        return value;
    }

    template <CSR Register>
    inline void write(xlen_t value) noexcept {
        asm volatile("csrw %0, %1" : : "i"(static_cast<u16_t>(Register)), "r"(value) : "memory");
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t swap(xlen_t value) noexcept {
        xlen_t previous;
        asm volatile("csrrw %0, %1, %2"
                     : "=r"(previous)
                     : "i"(static_cast<u16_t>(Register)), "r"(value)
                     : "memory");
        return previous;
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t set_bits(xlen_t bits) noexcept {
        xlen_t previous;
        asm volatile("csrrs %0, %1, %2"
                     : "=r"(previous)
                     : "i"(static_cast<u16_t>(Register)), "r"(bits)
                     : "memory");
        return previous;
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t clear_bits(xlen_t bits) noexcept {
        xlen_t previous;
        asm volatile("csrrc %0, %1, %2"
                     : "=r"(previous)
                     : "i"(static_cast<u16_t>(Register)), "r"(bits)
                     : "memory");
        return previous;
    }
}  // namespace riscv64::hal::csr
