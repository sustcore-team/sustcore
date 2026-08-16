/**
 * @file interrupt.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 IRQ Registry 的 RCU 快照发布与取消订阅语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 */

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

        void handle_test_interrupt(void *context, const device::interrupt::Event &event) noexcept {
            auto *count = static_cast<size_t *>(context);
            kernel::test::require(count != nullptr, "handler context 为空");
            kernel::test::require(event.line.source == device::interrupt::Source::SOFTWARE,
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

    void run_interrupt_registry(Context &) noexcept {
        using namespace device::interrupt;

        const Line test_line{.source = Source::SOFTWARE, .code = TEST_LINE_CODE};
        size_t handled = 0;

        require_error(subscribe(test_line, nullptr), tay::error_code::NULLPTR, "未拒绝空 handler");
        auto first = subscribe(test_line, handle_test_interrupt, &handled);
        kernel::test::require(first.has_value(), "首次订阅失败");
        require_error(subscribe(test_line, handle_test_interrupt, &handled),
                      tay::error_code::INVALID_ARGUMENT, "未拒绝重复订阅");
        kernel::test::require(dispatch(software_trap(TEST_LINE_CODE)) == DispatchResult::HANDLED,
                              "已订阅软件中断未被处理");
        kernel::test::require(handled == 1, "handler 执行次数错误");

        kernel::test::require(unsubscribe(*first).has_value(), "取消订阅失败");
        kernel::test::require(dispatch(software_trap(TEST_LINE_CODE)) == DispatchResult::UNHANDLED,
                              "取消订阅后仍然分发 handler");
        require_error(unsubscribe(*first), tay::error_code::OUT_OF_RANGE,
                      "重复取消未返回 OUT_OF_RANGE");

        auto replacement = subscribe(test_line, handle_test_interrupt, &handled);
        kernel::test::require(replacement.has_value(), "重新订阅失败");
        kernel::test::require(replacement->generation != first->generation,
                              "line 复用后 generation 未更新");
        require_error(unsubscribe(*first), tay::error_code::OUT_OF_RANGE, "旧 token 取消了新订阅");
        kernel::test::require(unsubscribe(*replacement).has_value(), "无法取消替换订阅");

        Subscription subscriptions[MAX_TEST_SUBSCRIPTIONS]{};
        size_t subscription_count = 0;
        bool capacity_reached     = false;
        for (; subscription_count < MAX_TEST_SUBSCRIPTIONS; ++subscription_count) {
            const Line line{.source = Source::SOFTWARE,
                            .code   = CAPACITY_LINE_BASE + subscription_count};
            auto subscription = subscribe(line, handle_test_interrupt, &handled);
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
}  // namespace kernel::test::cases
