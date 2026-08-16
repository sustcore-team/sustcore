/**
 * @file interrupt.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 与固件控制器无关的运行期 trap 中断分发表。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/interrupt.h>
#include <device/catalog.h>
#include <tay/err.h>
#include <tay/expected.h>

namespace device::interrupt {
    struct IrqClaim;

    class IrqDomain {
    public:
        virtual ~IrqDomain() = default;

        [[nodiscard]] virtual FirmwareId controller() const noexcept                 = 0;
        [[nodiscard]] virtual u32_t line_count() const noexcept                      = 0;
        [[nodiscard]] virtual tay::expected<u32_t, tay::error_code> claim() noexcept = 0;
        /** @brief 将控制器 claim 转换为已确认的活动 IRQ。 */
        [[nodiscard]] virtual tay::expected<void, tay::error_code> ack(
            const IrqClaim &claim) noexcept = 0;
        /** @brief 向控制器发出 end-of-interrupt，不等价于 complete。 */
        [[nodiscard]] virtual tay::expected<void, tay::error_code> eoi(
            const IrqClaim &claim) noexcept = 0;
        /** @brief 完成控制器上的活动 IRQ，使其可以再次被 claim。 */
        [[nodiscard]] virtual tay::expected<void, tay::error_code> complete(
            const IrqClaim &claim) noexcept = 0;
        [[nodiscard]] virtual tay::expected<void, tay::error_code> mask(
            u32_t hardware_irq) noexcept = 0;
        [[nodiscard]] virtual tay::expected<void, tay::error_code> unmask(
            u32_t hardware_irq) noexcept = 0;
        [[nodiscard]] virtual tay::expected<void, tay::error_code> set_priority(
            u32_t hardware_irq, u32_t priority) noexcept = 0;
    };

    struct IrqClaim {
        IrqDomain *domain  = nullptr;
        u32_t hardware_irq = 0;
        // 仅由当前 flow dispatch 生成，用于拒绝跨域或空 token 的完成请求。
        u32_t generation   = 0;

        [[nodiscard]] friend constexpr bool operator==(IrqClaim left,
                                                       IrqClaim right) noexcept = default;
    };

    struct CascadeFrame {
        IrqClaim parent{};
        IrqDomain *child_domain = nullptr;
    };

    enum class Source : u8_t { TIMER, SOFTWARE, EXTERNAL };

    struct Line {
        Source source;
        xlen_t code;

        [[nodiscard]] friend constexpr bool operator==(Line left, Line right) noexcept = default;
    };

    /** @brief 与架构 timer cause 编号无关的当前 CPU timer line。 */
    inline constexpr Line TIMER_LINE{.source = Source::TIMER, .code = 0};

    struct Event {
        Line line;
        const hal::TrapInfo &trap;
    };

    struct IrqLine {
        FirmwareId controller{};
        u32_t hardware_irq = 0;

        [[nodiscard]] friend constexpr bool operator==(IrqLine left,
                                                       IrqLine right) noexcept = default;
    };

    enum class IrqBindingKind : u8_t { LEAF, CASCADE };

    struct IrqBinding {
        IrqBindingKind kind     = IrqBindingKind::LEAF;
        IrqDomain *child_domain = nullptr;
        IrqLine line{};
    };

    using Handler    = void (*)(void *context, const Event &event) noexcept;
    using IrqHandler = void (*)(void *context, const IrqLine &line) noexcept;

    struct Subscription {
        Line line{};
        u32_t generation = 0;
    };

    struct IrqSubscription {
        IrqLine line{};
        u32_t generation = 0;
    };

    enum class DispatchResult : u8_t { HANDLED, UNHANDLED };

    /** @brief 将目录中的固件控制器引用解析为可交给控制器驱动的硬件 line。 */
    [[nodiscard]] tay::expected<IrqLine, tay::error_code> resolve(FirmwareId controller,
                                                                  u32_t hardware_irq) noexcept;

    /**
     * @brief 订阅一个架构 trap 源。
     * @note 当前第一阶段每条 line 只允许一个内核处理器。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     */
    [[nodiscard]] tay::expected<Subscription, tay::error_code> subscribe(
        Line line, Handler handler, void *context = nullptr) noexcept;

    /**
     * @brief 取消架构 trap 源订阅，并同步等待既有 handler 离开。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     * @post 成功返回后 context 可以立即销毁。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> unsubscribe(
        Subscription subscription) noexcept;

    /**
     * @brief 注册当前架构唯一的 root domain。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> register_domain(IrqDomain &domain) noexcept;
    /**
     * @brief 注销 domain，并同步等待所有既有 flow dispatch 离开。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     * @post 成功返回后 domain 可以立即销毁。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> unregister_domain(
        IrqDomain &domain) noexcept;
    /**
     * @brief 将 parent IRQ 绑定到 child domain，并由 flow handler 递归 drain。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     * @note 当前没有解除级联关系的接口，parent 和 child 必须保持有效。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> register_cascade(IrqDomain &parent,
                                                                        u32_t parent_irq,
                                                                        IrqDomain &child) noexcept;
    /**
     * @brief 订阅一个 domain hardware IRQ。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     */
    [[nodiscard]] tay::expected<IrqSubscription, tay::error_code> subscribe_irq(
        IrqLine line, IrqHandler handler, void *context = nullptr) noexcept;
    /**
     * @brief 取消 hardware IRQ 订阅，并同步等待既有 handler 离开。
     * @pre 不得在硬中断、IRQ handler 或仍持有 Registry RCU read guard 的路径调用。
     * @post 成功返回后 context 可以立即销毁。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> unsubscribe_irq(
        IrqSubscription subscription) noexcept;

    /** @brief 由通用 trap dispatcher 提交异步 trap。 */
    [[nodiscard]] DispatchResult dispatch(const hal::TrapInfo &info) noexcept;
}  // namespace device::interrupt
