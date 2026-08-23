/**
 * @file interrupt.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 IRQ Registry 的 RCU 快照发布与取消订阅语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <cpu/local.h>
#include <device/interrupt.h>
#include <test/cases.h>

namespace kernel::test::cases {
    namespace {
        constexpr size_t MAX_TEST_SUBSCRIPTIONS = 64;
        constexpr xlen_t TEST_LINE_CODE         = 0x52554355;
        constexpr xlen_t CAPACITY_LINE_BASE     = 0x10000;

        template <class T>
        void require_error(const T &result, tay::error_code expected,
                           const char *message) noexcept {
            kernel::test::require(!result && result.error() == expected, message);
        }

        void handle_test_irq(void *ctx, const device::interrupt::TrapEvent &event) noexcept {
            auto *count = static_cast<size_t *>(ctx);
            kernel::test::require(count != nullptr, "handler ctx 为空");
            kernel::test::require(event.line.source == device::interrupt::IrqSource::SOFTWARE,
                                  "handler 收到错误 source");
            ++*count;
        }

        [[nodiscard]] hal::TrapInfo software_trap(xlen_t code) noexcept {
            return hal::TrapInfo{
                .kind        = hal::TrapKind::SOFTWARE,
                .raw_cause   = code,
                .code        = code,
                .bad_address = 0,
                .user        = false,
                .access      = memory::FaultAccess::NONE,
            };
        }
    }  // namespace

    void run_irq_registry(Context &) noexcept {
        using namespace device::interrupt;

        const TrapLine test_line{.source = IrqSource::SOFTWARE, .code = TEST_LINE_CODE};
        size_t handled = 0;

        require_error(subscribe(test_line, nullptr), tay::error_code::NULLPTR, "未拒绝空 handler");
        auto first = subscribe(test_line, handle_test_irq, &handled);
        kernel::test::require(first.has_value(), "首次订阅失败");
        require_error(subscribe(test_line, handle_test_irq, &handled),
                      tay::error_code::INVALID_ARGUMENT, "未拒绝重复订阅");
        kernel::test::require(dispatch(software_trap(TEST_LINE_CODE)) == DispatchResult::HANDLED,
                              "已订阅软件中断未被处理");
        kernel::test::require(handled == 1, "handler 执行次数错误");

        kernel::test::require(unsubscribe(*first).has_value(), "取消订阅失败");
        kernel::test::require(dispatch(software_trap(TEST_LINE_CODE)) == DispatchResult::UNHANDLED,
                              "取消订阅后仍然分发 handler");
        require_error(unsubscribe(*first), tay::error_code::OUT_OF_RANGE,
                      "重复取消未返回 OUT_OF_RANGE");

        auto replacement = subscribe(test_line, handle_test_irq, &handled);
        kernel::test::require(replacement.has_value(), "重新订阅失败");
        kernel::test::require(replacement->gen != first->gen, "line 复用后 gen 未更新");
        require_error(unsubscribe(*first), tay::error_code::OUT_OF_RANGE, "旧 token 取消了新订阅");
        kernel::test::require(unsubscribe(*replacement).has_value(), "无法取消替换订阅");

        TrapSub subscriptions[MAX_TEST_SUBSCRIPTIONS]{};
        size_t subscription_count = 0;
        bool capacity_reached     = false;
        for (; subscription_count < MAX_TEST_SUBSCRIPTIONS; ++subscription_count) {
            const TrapLine line{.source = IrqSource::SOFTWARE,
                                .code   = CAPACITY_LINE_BASE + subscription_count};
            auto subscription = subscribe(line, handle_test_irq, &handled);
            if (!subscription) {
                kernel::test::require(subscription.error() == tay::error_code::OVERFLOW_ERROR,
                                      "容量耗尽返回了错误的 error_code");
                capacity_reached = true;
                break;
            }
            subscriptions[subscription_count] = *subscription;
        }
        kernel::test::require(capacity_reached, "未在固定容量内报告 OVERFLOW_ERROR");
        kernel::test::require(subscription_count != 0, "容量测试未成功发布任何订阅");
        kernel::test::require(
            dispatch(software_trap(subscriptions[0].line.code)) == DispatchResult::HANDLED,
            "容量测试订阅无法分发");

        for (size_t index = 0; index < subscription_count; ++index)
            kernel::test::require(unsubscribe(subscriptions[index]).has_value(),
                                  "容量测试清理失败");
        kernel::test::require(
            dispatch(software_trap(subscriptions[0].line.code)) == DispatchResult::UNHANDLED,
            "容量测试清理后仍然分发 handler");
    }

    void run_preempt_guard(Context &) noexcept {
        kernel::test::require(cpu::current_id() == cpu::CpuId{0},
                              "BSP CpuLocal 未在抢占测试前绑定");
        kernel::test::require(!cpu::preempt_disabled(), "抢占测试开始时深度非零");
        {
            hal::preempt_guard outer;
            kernel::test::require(cpu::preempt_disabled(), "外层 guard 未禁止抢占");
            {
                hal::preempt_guard inner;
                kernel::test::require(cpu::preempt_disabled(), "嵌套 guard 提前恢复抢占");
            }
            kernel::test::require(cpu::preempt_disabled(), "内层 guard 退出后抢占提前恢复");
        }
        kernel::test::require(!cpu::preempt_disabled(), "最外层 guard 未恢复抢占资格");

        // IRQ 关闭时退出最外层 guard 只能留下 deferred 标记，不能在硬中断上下文切换；
        // 这里验证请求不会丢失，真正的 checkpoint 由 IRQ 恢复路径负责。
        {
            hal::irq_guard irq;
            {
                hal::preempt_guard guard;
                cpu::defer_resched();
            }
            kernel::test::require(cpu::take_resched(), "IRQ 关闭期间 deferred 请求未保留");
        }
    }
}  // namespace kernel::test::cases
