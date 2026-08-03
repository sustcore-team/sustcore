/**
 * @file smp.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI RISC-V 次级 CPU 启动后端
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/smp.h>
#include <sbi/sbi.h>
#include <sustcore/addrspace.h>

#include <atomic>

extern "C" char sbi_secondary_trampoline[];

namespace riscv64::boot::smp {
    bool supports_secondary_start() noexcept {
        static std::atomic<int> cached{-1};
        // release/acquire 发布首次 SBI 探测结果，避免多个 CPU 重复执行固件调用。
        const int value = cached.load(std::memory_order_acquire);
        if (value >= 0)
            return value != 0;

        const auto probe     = sbi_probe_extension(SBI_EID_HSM);
        const bool available = probe.error == SBI_SUCCESS && probe.value != 0;
        cached.store(available ? 1 : 0, std::memory_order_release);
        return available;
    }

    tay::expected<void, tay::error_code> start_secondary(cpu::cpu_hwid_t hardware_id,
                                                         PhyAddr arguments_physical) noexcept {
        if (!supports_secondary_start())
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        if (arguments_physical.arith() == 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        const auto status =
            sbi_ecall(SBI_EID_HSM, SBI_HART_GET_STATUS, hardware_id.value, 0, 0, 0, 0, 0);
        if (status.error != SBI_SUCCESS || status.value != STOPPED)
            return tay::Err(tay::error_code::OUT_OF_RANGE);

        const auto trampoline_physical =
            reinterpret_cast<addr_t>(sbi_secondary_trampoline) - KVA_START;
        const auto result = sbi_ecall(SBI_EID_HSM, SBI_HART_START, hardware_id.value,
                                      trampoline_physical, arguments_physical.arith(), 0, 0, 0);
        if (result.error != SBI_SUCCESS)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        return {};
    }

    PhyAddr identity_trampoline_page() noexcept {
        constexpr addr_t PAGE_MASK = ~(PAGE_SIZE - 1);
        return PhyAddr((reinterpret_cast<addr_t>(sbi_secondary_trampoline) - KVA_START) &
                       PAGE_MASK);
    }
}  // namespace riscv64::boot::smp
