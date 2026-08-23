/**
 * @file ipi.cpp
 * @brief LoongArch IOCSR 运行期 IPI 后端。
 */

#include <arch/csr.h>
#include <arch/loongarch64/namespace.h>
#include <arch/loongarch64/valdef.h>
#include <arch/smp.h>

namespace loongarch64::hal {
    namespace {
        // LABOOT 保留 vector 0 作为 AP 启动门铃。运行期只使用 vector 1，避免启动协议和
        // 已运行 CPU 的 mailbox 共享同一 pending 位。IOCSR_IPI_SEND 编码 vector number，
        // 而 IPI_EN/IPI_CLEAR 使用对应 bitmask，二者不可混用。
        constexpr u32_t RUNTIME_IPI_VECTOR = 1;
        constexpr u32_t RUNTIME_IPI_MASK   = 1U << RUNTIME_IPI_VECTOR;
        constexpr u64_t MAX_IPI_CPU_ID     = (u64_t{1} << 10) - 1;
        constexpr xlen_t ECFG_IPI          = xlen_t{1} << 12;
    }  // namespace

    void init_ipi() noexcept {
        csr::iocsr_write32(IOCSR_IPI_CLEAR, RUNTIME_IPI_MASK);
        const auto enabled = csr::iocsr_read32(IOCSR_IPI_EN);
        csr::iocsr_write32(IOCSR_IPI_EN, enabled | RUNTIME_IPI_MASK);
        (void)csr::set_bits<csr::CSR::ECFG>(ECFG_IPI);
    }

    tay::expected<void, tay::error_code> send_ipi(cpu::CpuHwId target) noexcept {
        if (target.value > MAX_IPI_CPU_ID)
            return tay::Err(tay::error_code::OUT_OF_RANGE);

        const auto command = (static_cast<u32_t>(target.value) << IOCSR_IPI_SEND_CPU_SHIFT) |
                             (RUNTIME_IPI_VECTOR << IOCSR_IPI_SEND_IP_SHIFT);
        csr::iocsr_write32(IOCSR_IPI_SEND, command);
        return {};
    }

    void ack_ipi() noexcept {
        csr::iocsr_write32(IOCSR_IPI_CLEAR, RUNTIME_IPI_MASK);
    }
}  // namespace loongarch64::hal
