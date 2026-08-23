/**
 * @file csr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 控制状态寄存器访问
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>
#include <tay/bits.h>

namespace loongarch64::hal::csr {
    enum class CSR : u16_t {
        CRMD       = 0x00,
        PRMD       = 0x01,
        EUEN       = 0x02,
        ECFG       = 0x04,
        ESTAT      = 0x05,
        ERA        = 0x06,
        BADV       = 0x07,
        EENTRY     = 0x0c,
        ASID       = 0x11,
        PGDL       = 0x19,
        PGDH       = 0x1a,
        PGD        = 0x1b,
        PWCTL0     = 0x1c,
        PWCTL1     = 0x1d,
        STLBPGSIZE = 0x1e,
        TCFG       = 0x41,
        TVAL       = 0x42,
        CNTC       = 0x43,
        TICLR      = 0x44,
        TLBRENTRY  = 0x88,
        TLBRRERA   = 0x8a,
        TLBRSAVE   = 0x8b,
        SAVE1      = 0x31,
        DMWIN0     = 0x180,
        DMWIN1     = 0x181,
        DMWIN2     = 0x182,
        DMWIN3     = 0x183,
    };

    template <CSR Register>
    [[nodiscard]] inline xlen_t read() noexcept {
        xlen_t value;
        asm volatile("csrrd %0, %1" : "=r"(value) : "i"(static_cast<u16_t>(Register)));
        return value;
    }

    template <CSR Register>
    inline void write(xlen_t value) noexcept {
        asm volatile("csrwr %0, %1" : "+r"(value) : "i"(static_cast<u16_t>(Register)) : "memory");
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t swap(xlen_t value) noexcept {
        asm volatile("csrwr %0, %1" : "+r"(value) : "i"(static_cast<u16_t>(Register)) : "memory");
        return value;
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t exchange_bits(xlen_t value, xlen_t mask) noexcept {
        asm volatile("csrxchg %0, %1, %2"
                     : "+r"(value)
                     : "r"(mask), "i"(static_cast<u16_t>(Register))
                     : "memory");
        return value;
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t set_bits(xlen_t bits) noexcept {
        return exchange_bits<Register>(bits, bits);
    }

    template <CSR Register>
    [[nodiscard]] inline xlen_t clear_bits(xlen_t bits) noexcept {
        return exchange_bits<Register>(0, bits);
    }

    [[nodiscard]] inline u64_t iocsr_read64(addr_t addr) noexcept {
        u64_t value;
        asm volatile("iocsrrd.d %0, %1" : "=r"(value) : "r"(addr));
        return value;
    }

    [[nodiscard]] inline u32_t iocsr_read32(addr_t addr) noexcept {
        u32_t value;
        asm volatile("iocsrrd.w %0, %1" : "=r"(value) : "r"(addr));
        return value;
    }

    inline void iocsr_write64(addr_t addr, u64_t value) noexcept {
        asm volatile("iocsrwr.d %0, %1" : : "r"(value), "r"(addr) : "memory");
    }

    inline void iocsr_write32(addr_t addr, u32_t value) noexcept {
        asm volatile("iocsrwr.w %0, %1" : : "r"(value), "r"(addr) : "memory");
    }
}  // namespace loongarch64::hal::csr
