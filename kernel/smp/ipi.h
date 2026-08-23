/**
 * @file ipi.h
 * @brief Runtime IPI 的通用 mailbox、原因分派和诊断计数。
 */

#pragma once

#include <cpu/local.h>
#include <tay/expected.h>

namespace smp {
    enum class IpiReason : u32_t {
        RESCHEDULE     = 1U << 0,
        TLB_SHOOTDOWN  = 1U << 1,
        STOP           = 1U << 2,
        TIMER_DEADLINE = 1U << 3,
    };

    /**
     * @brief 某个逻辑 CPU 的 IPI mailbox 诊断快照。
     * @note 计数仅用于诊断和 selftest；不会参与 IPI 协议正确性。
     */
    struct IpiStats final {
        u64_t posted               = 0;
        u64_t coalesced            = 0;
        u64_t notifications_needed = 0;
        u64_t dispatches           = 0;
        u64_t reschedules          = 0;
        u64_t tlb_shootdowns       = 0;
        u64_t timer_deadlines      = 0;
        u64_t stops                = 0;
    };

    using TlbShootdownHandler = void (*)() noexcept;

    /**
     * @brief 向目标 CPU 的 mailbox 合并原因位。
     * @return 本次是否新增至少一个原因位，调用方据此决定是否调用架构 send_ipi()。
     * @pre 普通原因的 target 必须是 online CPU；STOP 也允许发送到已 started、尚未
     *      online 的 bring-up CPU。
     */
    [[nodiscard]] bool post(cpu::CpuId target, IpiReason reason) noexcept;

    /**
     * @brief 发布原因并在需要时向目标发送架构 IPI。
     * @note send_ipi() 失败时 mailbox 原因仍保留，调用方必须将错误升级为不可恢复的运行期
     *       故障，不能继续假设目标 CPU 已经观察到该请求。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> request(cpu::CpuId target,
                                                               IpiReason reason) noexcept;

    /** @brief 返回固定 mailbox 的未处理原因位，仅用于 selftest 和故障诊断。 */
    [[nodiscard]] u32_t pending_reasons(cpu::CpuId target) noexcept;

    /** @brief 复制目标 CPU 的 IPI 诊断计数。 */
    [[nodiscard]] IpiStats stats(cpu::CpuId target) noexcept;

    /**
     * @brief 注册 TLB shootdown 的本地 IRQ handler。
     * @pre 只能在开始发送 TLB_SHOOTDOWN IPI 前调用一次；handler 不得分配、阻塞或获取
     *      page-table/coordinator 锁。
     */
    void set_tlb_handler(TlbShootdownHandler handler) noexcept;

    /**
     * @brief 在当前 CPU 的软件中断路径消费 mailbox。
     * @note 架构层必须先确认本地软件中断源，再调用本函数。该函数绝不进行上下文切换；
     *       RESCHEDULE 只请求 trap-return 的调度检查。STOP 优先且不会返回。
     */
    void dispatch_ipi() noexcept;
}  // namespace smp
