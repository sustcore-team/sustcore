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

namespace boot::smp {
    namespace {
        // 此对象在启动运行时可达，必须保持常量初始化。
        constinit std::atomic<int> ap_start_support{-1};
    }  // namespace

    bool supports_ap_start() noexcept {
        // release/acquire 发布已缓存的 SBI 探测结果；并发首次调用仍可能重复探测。
        // OpenSBI 的“Platform HSM Device : ---”只描述平台设备树状况，不能替代
        // probe_extension：QEMU virt 可以没有专用 HSM 设备，但仍提供 SBI HSM。
        const int value = ap_start_support.load(std::memory_order_acquire);
        if (value >= 0)
            return value != 0;

        const auto probe     = sbi_probe_extension(SBI_EID_HSM);
        const bool available = probe.error == SBI_SUCCESS && probe.value != 0;
        ap_start_support.store(available ? 1 : 0, std::memory_order_release);
        return available;
    }

    tay::expected<void, tay::error_code> start_ap(cpu::CpuHwId hw_id,
                                                  PhyAddr arguments_physical) noexcept {
        if (!supports_ap_start())
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        if (arguments_physical.arith() == 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        const auto status = sbi_ecall(SBI_EID_HSM, SBI_HART_GET_STATUS, hw_id.value, 0, 0, 0, 0, 0);
        if (status.error != SBI_SUCCESS || status.value != STOPPED)
            return tay::Err(tay::error_code::OUT_OF_RANGE);

        const auto trampoline_physical =
            reinterpret_cast<addr_t>(sbi_secondary_trampoline) - KVA_START;
        const auto result = sbi_ecall(SBI_EID_HSM, SBI_HART_START, hw_id.value, trampoline_physical,
                                      arguments_physical.arith(), 0, 0, 0);
        if (result.error != SBI_SUCCESS)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        return {};
    }

    PhyAddr trampoline_page() noexcept {
        constexpr addr_t PAGE_MASK = ~(PAGE_SIZE - 1);
        return PhyAddr((reinterpret_cast<addr_t>(sbi_secondary_trampoline) - KVA_START) &
                       PAGE_MASK);
    }
}  // namespace boot::smp
