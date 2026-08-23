/**
 * @file hrtimer.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief BSP 非拥有式 precision timer priority queue。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <synchronized.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <timer/deadline.h>
#include <error/timer.h>
#include <timer/node.h>

namespace kernel::async {
    class Worklet;
}  // namespace kernel::async

namespace kernel::timer {
    /** @brief cancel() 与 timer 到期路径竞争的结果。 */
    enum class CancelResult : u8_t {
        /** timer 仍在 heap 中，cancel 已摘链并将 completion post 到 WorkQueue。 */
        CANCELLED,
        /** timer 已经到期或 post，cancel 不再改变 completion 归属。 */
        RACE_LOST,
        /** node 未 arm、已 retire，或不属于当前 engine。 */
        NOT_ARMED,
    };

    /**
     * @brief 借用 HrTimer 和 typed Worklet 的 BSP precision timer engine。
     *
     * engine 内部以 u64_t 纳秒值维护 heap，公开时间边界使用 units::time。arm()
     * 使 Worklet 进入 RESERVED；到期或成功 cancel 只把它转为 QUEUED 并 post 到固定
     * BSP WorkQueue，IRQ 路径不执行虚函数。具体 Worklet 必须在可能销毁宿主前依次
     * retire() 和 reset() node。engine 不分配、移动或销毁 node、Worklet 及其宿主。
     */
    class HRTQueue final {
    public:
        constexpr HRTQueue() noexcept         = default;
        HRTQueue(const HRTQueue &)            = delete;
        HRTQueue &operator=(const HRTQueue &) = delete;

        /** @brief 绑定固定生命期的 BSP deadline coordinator，只能调用一次。 */
        void initialize(DeadlineMux &deadlines) noexcept;

        /**
         * @brief 借用 IDLE node，并独占保留 IDLE completion 直至 WorkQueue dispatch。
         * @param node 宿主嵌入的 timer node；必须保持地址稳定。
         * @param deadline 架构单调时钟 epoch 上的绝对到期时刻。
         * @param completion 已完成 typed 字段配置的 Worklet。
         * @return 成功时 node 进入 QUEUED、completion 进入 RESERVED；前置条件不满足时
         * 返回 INVALID_ARGUMENT，且不借用两者。
         */
        [[nodiscard]] tay::expected<void, TimerError> arm(HrTimer &node, units::time deadline,
                                                          async::Worklet &completion) noexcept;

        /**
         * @brief 尝试取消 QUEUED node；成功取消仍会异步执行原 completion 完成清理。
         * @note 本函数可与 IRQ 到期路径竞争，返回值表示谁取得 QUEUED -> ELAPSED
         * 转换，不表示 Worklet 已经执行。
         */
        [[nodiscard]] CancelResult cancel(HrTimer &node) noexcept;

        /**
         * @brief 摘除所有 deadline <= now 的 node，并将其 RESERVED completion post 到 WorkQueue。
         * @note 可在本地 timer IRQ 中调用；不分配、不阻塞、不执行 Worklet 虚函数。
         */
        void progress(units::time now) noexcept;

        /**
         * @brief 在 deadline owner CPU 的 IPI 路径提交远端 arm/cancel 后的 heap root。
         * @pre 当前 CPU 必须是 initialize() 时绑定的 BSP deadline owner。
         */
        void refresh_from_ipi() noexcept;

        /** @brief Worklet 完成路径将 POSTED node 转为 RETIRED。 */
        void retire(HrTimer &node) noexcept;

        /** @brief 将 RETIRED node 清空为 IDLE，使宿主可以销毁或重新 arm。 */
        void reset(HrTimer &node) noexcept;

        [[nodiscard]] hal::TimerDeadline root_deadline() noexcept;
        [[nodiscard]] HRTState state(const HrTimer &node) noexcept;
        [[nodiscard]] size_t size() noexcept;

    private:
        struct State final {
            constexpr State() noexcept : heap(timer_heap::locate_type{}, TIMER_HEAP_CMP) {}

            timer_heap heap;
            DeadlineMux *deadlines = nullptr;
            cpu::CpuId deadline_cpu{cpu::INVALID_CPU};
            bool root_deadline_dirty         = false;
            // 每次远端 heap 修改递增；owner IPI 只可清除它已发布的那一代，避免
            // 在合并通知窗口内误清除随后到达的 deadline 更新。
            u64_t remote_deadline_generation = 0;
        };

        [[nodiscard]] static hal::TimerDeadline root_locked(const State &state) noexcept;
        [[nodiscard]] static bool sync_root_locked(State &state) noexcept;
        static void kick_owner(cpu::CpuId owner) noexcept;
        void post_elapsed(HrTimer &node) noexcept;

        kernel::irq_simple_synchronized<State> state_{};
    };

    [[nodiscard]] HRTQueue &bsp_hrtimers() noexcept;
}  // namespace kernel::timer
