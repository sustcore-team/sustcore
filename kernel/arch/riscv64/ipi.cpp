/**
 * @file ipi.cpp
 * @brief RISC-V SBI IPI 与 SSIP 确认后端。
 */

#include <arch/csr.h>
#include <arch/riscv64/namespace.h>
#include <arch/smp.h>
#include <sbi/sbi.h>

namespace riscv64::hal {
    namespace {
        constexpr xlen_t SSIP_BIT           = xlen_t{1} << 1;
        constexpr u64_t HARTS_PER_MASK_WORD = 64;
    }  // namespace

    void init_ipi() noexcept {
        // SBI IPI 以 supervisor software interrupt 形式抵达；先确认可能的陈旧 pending，
        // 再开放 SSIP，避免启动阶段遗留的通知在 mailbox 建立前进入 trap。
        (void)csr::clear_bits<csr::CSR::SIP>(SSIP_BIT);
        (void)csr::set_bits<csr::CSR::SIE>(SSIP_BIT);
    }

    tay::expected<void, tay::error_code> send_ipi(cpu::CpuHwId target) noexcept {
        const u64_t base      = target.value & ~(HARTS_PER_MASK_WORD - 1);
        const u64_t offset    = target.value - base;
        const u64_t hart_mask = u64_t{1} << offset;
        const auto result = sbi_send_ipi(static_cast<xlen_t>(hart_mask), static_cast<xlen_t>(base));
        if (result.error != SBI_SUCCESS)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        return {};
    }

    void ack_ipi() noexcept {
        (void)csr::clear_bits<csr::CSR::SIP>(SSIP_BIT);
    }
}  // namespace riscv64::hal
