/**
 * @file smp.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 多处理器架构支持
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/csr.h>
#include <arch/smp.h>
#include <sbi/sbi.h>

namespace riscv64::smp {
    bool init_ipi() noexcept {
        (void)hal::csr::set_bits<hal::csr::CSR::SIE>(xlen_t{1} << 1);
        return true;
    }

    bool send_ipi(cpu::cpu_hwid_t hardware_id) noexcept {
        constexpr u64_t HARTS_PER_MASK = 64;
        const auto base                = hardware_id.value & ~(HARTS_PER_MASK - 1);
        const auto mask                = u64_t{1} << (hardware_id.value - base);
        return sbi_send_ipi(mask, base).error == SBI_SUCCESS;
    }

    void clear_ipi() noexcept {
        (void)hal::csr::clear_bits<hal::csr::CSR::SIP>(xlen_t{1} << 1);
    }
}  // namespace riscv64::smp
