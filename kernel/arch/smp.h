/**
 * @file smp.h
 * @brief 运行期 IPI 的架构窄接口。
 */

#pragma once

#include <arch/namespace.h>
#include <cpu/local.h>
#include <tay/err.h>
#include <tay/expected.h>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    /** @brief 在当前 CPU 使能并确认运行期 IPI 源。 */
    void init_ipi() noexcept;

    /**
     * @brief 向指定硬件 CPU 标识发送运行期 IPI。
     * @note 调用者必须先以 release 语义发布目标 mailbox；该函数不分配、不阻塞。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> send_ipi(cpu::CpuHwId target) noexcept;

    /** @brief 确认当前 CPU 的运行期 IPI 硬件源。 */
    void ack_ipi() noexcept;
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
