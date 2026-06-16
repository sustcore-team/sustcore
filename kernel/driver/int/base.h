/**
 * @file int.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief intterrupt
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <driver/clock.h>
#include <driver/base.h>
#include <logger.h>
#include <sus/owner.h>
#include <sus/units.h>
#include <device/device.h>
#include <sustcore/errcode.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class InterruptGuard {
private:
    bool entered      = false;
    bool prev_enabled = false;

public:
    InterruptGuard() = default;

    /**
     * @brief 进入中断关闭保护区.
     */
    void enter() {
        if (entered) {
            return;
        }
        prev_enabled = Interrupt::enabled();
        Interrupt::cli();
        entered = true;
    }

    /**
     * @brief 退出保护区并恢复中断状态.
     */
    ~InterruptGuard() {
        if (entered && prev_enabled) {
            Interrupt::sti();
        }
    }
};

namespace driver {
    class IrqDomain;
    class IrqManager;

    /**
     * @brief 中断触发方式.
     */
    enum class IrqTrigger { EDGE_RISING, EDGE_FALLING, LEVEL_HIGH, LEVEL_LOW };

    /**
     * @brief 中断控制器抽象接口.
     */
    class IrqChip : public DriverBase {
    public:
        /**
         * @brief 使用驱动资源包构造中断控制器对象.
         *
         * @param res 设备节点与资源集合.
         */
        explicit IrqChip(DevRes res) noexcept : DriverBase(std::move(res)) {}

        /**
         * @brief 销毁中断控制器对象.
         */
        virtual ~IrqChip() noexcept = default;
        [[nodiscard]]
        virtual Result<void> enable_irq(hwirq_t hw_irq) noexcept = 0;
        [[nodiscard]]
        virtual Result<void> disable_irq(hwirq_t hw_irq) noexcept = 0;
        [[nodiscard]]
        virtual Result<void> set_priority(hwirq_t hw_irq,
                                          domain_t prio) noexcept = 0;
        [[nodiscard]]
        virtual Result<void> set_affinity(hwirq_t hw_irq,
                                          cpu_mask_t mask) noexcept = 0;
        [[nodiscard]]
        virtual Result<void> ack(const IrqEvent &event) noexcept = 0;
        [[nodiscard]]
        virtual Result<void> set_trigger(hwirq_t hw_irq,
                                         IrqTrigger trigger) noexcept = 0;
        /**
         * @brief 将当前中断控制器挂接到父中断域.
         *
         * 默认实现表示当前芯片不需要父域挂接. 具备级联关系的控制器可覆写该接口,
         * 在此完成父域 virq 的 handler 注册与二级分发接入.
         *
         * @param irqman 全局中断管理器.
         * @param self_domain 当前芯片所属的中断域.
         * @return Result<void> 挂接结果.
         */
        [[nodiscard]]
        virtual Result<void> attach_to_parent_domain(
            IrqManager &irqman, IrqDomain &self_domain) noexcept {
            (void)irqman;
            (void)self_domain;
            void_return();
        }
    };

    /**
     * @brief 中断域与硬件中断号的解析结果.
     */
    struct IrqResolveResult {
        domain_t domain = INVALID_DOMAIN_ID;
        hwirq_t hw_irq  = 0;
    };

    /**
     * @brief 一次 virq 分发时传递给处理函数的上下文.
     */
    struct IrqEvent {
        // 对应的 virq
        virq_t virq    = 0;
        // 对应的 hw_irq
        hwirq_t hw_irq = 0;
        // 对应的 IrqDomain
        IrqDomain *domain = nullptr;
        // 保留: 供IrqChip使用, 以保存特殊的事件上下文
        qword chip_specific[2];
    };

    /**
     * @brief 中断域抽象接口.
     */
    class IrqDomain {
    public:
        virtual ~IrqDomain() = default;
        [[nodiscard]]
        virtual domain_t id() const noexcept = 0;
        [[nodiscard]]
        virtual const char *name() const noexcept = 0;
        [[nodiscard]]
        virtual IrqChip &chip() const noexcept = 0;
        [[nodiscard]]
        virtual Result<virq_t> bind(hwirq_t hw_irq, virq_t virq) = 0;
        [[nodiscard]]
        virtual Result<virq_t> to_virq(hwirq_t hw_irq) const = 0;
        [[nodiscard]]
        virtual Result<hwirq_t> to_hwirq(virq_t virq) const = 0;
        [[nodiscard]]
        virtual bool contains(virq_t virq) const noexcept = 0;
        [[nodiscard]]
        virtual bool supports(hwirq_t hw_irq) const noexcept = 0;
        [[nodiscard]]
        virtual Result<void> enable(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> disable(hwirq_t hw_irq) = 0;
        [[nodiscard]]
        virtual Result<void> ack(const IrqEvent &event) = 0;
        [[nodiscard]]
        virtual Result<void> set_priority(hwirq_t hw_irq, domain_t prio) = 0;
        [[nodiscard]]
        virtual Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) = 0;
        [[nodiscard]]
        virtual Result<void> set_trigger(hwirq_t hw_irq,
                                         IrqTrigger trigger) = 0;
    };

    /**
     * @brief 线性 hw_irq 编号的固定大小中断域.
     *
     * @tparam MAX_HW_IRQ 域内可支持的最大硬件中断号数量.
     */
    template <size_t MAX_HW_IRQ>
    class LinearIrqDomain final : public IrqDomain {
    public:
        /**
         * @brief 构造一个线性中断域.
         *
         * @param identifier 域 ID.
         * @param domain_name 域名称.
         * @param chip 后端中断芯片引用.
         */
        LinearIrqDomain(domain_t identifier, std::string domain_name,
                        IrqChip &chip) noexcept
            : _id(identifier),
              _name(std::move(domain_name)),
              _chip(&chip),
              _virq_to_hwirq() {
            for (auto &slot : _virqs) {
                slot = std::nullopt;
            }
        }

        [[nodiscard]]
        domain_t id() const noexcept override {
            return _id;
        }

        [[nodiscard]]
        const char *name() const noexcept override {
            return _name.c_str();
        }

        [[nodiscard]]
        IrqChip &chip() const noexcept override {
            return *_chip;
        }

        [[nodiscard]]
        Result<virq_t> bind(hwirq_t hw_irq, virq_t virq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            auto &slot = _virqs[hw_irq];
            if (slot.has_value()) {
                if (*slot != virq) {
                    unexpect_return(ErrCode::KEY_DUPLICATED);
                }
                return *slot;
            }
            if (_virq_to_hwirq.contains(virq)) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }

            slot                 = virq;
            _virq_to_hwirq[virq] = hw_irq;
            return virq;
        }

        [[nodiscard]]
        Result<virq_t> to_virq(hwirq_t hw_irq) const override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (!_virqs[hw_irq].has_value()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return *_virqs[hw_irq];
        }

        [[nodiscard]]
        Result<hwirq_t> to_hwirq(virq_t virq) const override {
            auto it = _virq_to_hwirq.find(virq);
            if (it == _virq_to_hwirq.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return it->second;
        }

        [[nodiscard]]
        bool contains(virq_t virq) const noexcept override {
            return _virq_to_hwirq.contains(virq);
        }

        [[nodiscard]]
        bool supports(hwirq_t hw_irq) const noexcept override {
            return static_cast<size_t>(hw_irq) < MAX_HW_IRQ;
        }

        [[nodiscard]]
        Result<void> enable(hwirq_t hw_irq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->enable_irq(hw_irq);
        }

        [[nodiscard]]
        Result<void> disable(hwirq_t hw_irq) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->disable_irq(hw_irq);
        }

        [[nodiscard]]
        Result<void> ack(const IrqEvent &event) override {
            if (!supports(event.hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->ack(event);
        }

        [[nodiscard]]
        Result<void> set_priority(hwirq_t hw_irq, domain_t prio) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->set_priority(hw_irq, prio);
        }

        [[nodiscard]]
        Result<void> set_affinity(hwirq_t hw_irq, cpu_mask_t mask) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->set_affinity(hw_irq, mask);
        }

        [[nodiscard]]
        Result<void> set_trigger(hwirq_t hw_irq, IrqTrigger trigger) override {
            if (!supports(hw_irq)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return _chip->set_trigger(hw_irq, trigger);
        }

    private:
        domain_t _id = INVALID_DOMAIN_ID;
        std::string _name;
        IrqChip *_chip = nullptr;
        std::optional<virq_t> _virqs[MAX_HW_IRQ]{};
        std::unordered_map<virq_t, hwirq_t> _virq_to_hwirq;
    };

    /**
     * @brief 全局中断管理器.
     */
    class IrqManager {
    private:

        std::unordered_map<domain_t, util::owner<IrqDomain *>> _domains;
        std::unordered_map<virq_t, IrqResolveResult> _virq_map;
        std::unordered_map<virq_t, IrqHandler> _handlers;
        virq_t _next_virq = 1;
    public:
        IrqManager() = default;

        /**
         * @brief 注册一个中断域.
         */
        Result<void> register_domain(util::owner<IrqDomain *> domain) {
            if (domain == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (_domains.find(domain->id()) != _domains.end()) {
                loggers::INTERRUPT::ERROR("中断域 ID: %u 已存在!",
                                          domain->id());
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            loggers::INTERRUPT::INFO("注册中断域: %s (ID: %u)", domain->name(),
                                     domain->id());
            _domains[domain->id()] = std::move(domain);
            void_return();
        }

        /**
         * @brief 为指定域中的 hw_irq 分配稳定 virq.
         *
         * @param domain_id 中断域 ID.
         * @param hw_irq 域内硬件中断号.
         * @return Result<virq_t> 对应的全局 virq.
         */
        [[nodiscard]]
        Result<virq_t> allocate_virq(domain_t domain_id, hwirq_t hw_irq) {
            auto domain_res = get_domain(domain_id);
            propagate(domain_res);

            auto current_res = domain_res.value().get().to_virq(hw_irq);
            if (current_res.has_value()) {
                return current_res.value();
            }
            if (current_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                propagate_return(current_res);
            }

            virq_t virq   = _next_virq++;
            auto bind_res = domain_res.value().get().bind(hw_irq, virq);
            propagate(bind_res);
            _virq_map[virq] = IrqResolveResult{
                .domain = domain_id,
                .hw_irq = hw_irq,
            };
            return virq;
        }

        /**
         * @brief 按域 ID 获取域.
         */
        [[nodiscard]]
        Result<IrqDomain &> get_domain(domain_t identifier) const {
            auto it = _domains.find(identifier);
            if (it == _domains.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return std::ref(*it->second);
        }

        /**
         * @brief 通过 virq 解析所属中断域.
         */
        [[nodiscard]]
        Result<IrqDomain &> find_domain(virq_t virq) const {
            auto resolve_res = resolve(virq);
            propagate(resolve_res);
            return get_domain(resolve_res.value().domain);
        }

        /**
         * @brief 解析 virq 到 domain 与 hw_irq.
         */
        [[nodiscard]]
        Result<IrqResolveResult> resolve(virq_t virq) const {
            auto it = _virq_map.find(virq);
            if (it == _virq_map.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return it->second;
        }

        /**
         * @brief 使能全局 virq.
         */
        [[nodiscard]]
        Result<void> enable_irq(virq_t virq) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().enable(resolved.value().hw_irq);
        }

        /**
         * @brief 屏蔽全局 virq.
         */
        [[nodiscard]]
        Result<void> disable_irq(virq_t virq) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().disable(resolved.value().hw_irq);
        }

        /**
         * @brief 应答 IrqEvent
         */
        [[nodiscard]]
        Result<void> ack(const IrqEvent &event) {
            auto resolved = resolve(event.virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().ack(event);
        }

        /**
         * @brief 设置全局 virq 优先级.
         */
        [[nodiscard]]
        Result<void> set_priority(virq_t virq, domain_t prio) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().set_priority(resolved.value().hw_irq,
                                                     prio);
        }

        /**
         * @brief 设置全局 virq 亲和性.
         */
        [[nodiscard]]
        Result<void> set_affinity(virq_t virq, cpu_mask_t mask) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().set_affinity(resolved.value().hw_irq,
                                                     mask);
        }

        /**
         * @brief 设置全局 virq 的触发方式.
         */
        [[nodiscard]]
        Result<void> set_trigger(virq_t virq, IrqTrigger trigger) {
            auto resolved = resolve(virq);
            propagate(resolved);
            auto domain = get_domain(resolved.value().domain);
            propagate(domain);
            return domain.value().get().set_trigger(resolved.value().hw_irq,
                                                    trigger);
        }

        /**
         * @brief 为指定 virq 注册处理函数.
         *
         * @param virq 目标 virq.
         * @param handler 处理函数.
         * @return Result<void> 注册结果.
         */
        Result<void> register_handler(virq_t virq, IrqHandler handler) {
            loggers::INTERRUPT::INFO("注册 virq handler: virq=%llu", (unsigned long long)virq);
            auto resolve_res = resolve(virq);
            propagate(resolve_res);
            if (_handlers.contains(virq)) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            _handlers[virq] = std::move(handler);
            void_return();
        }

        /**
         * @brief 注销指定 virq 的处理函数.
         *
         * @param virq 目标 virq.
         * @return Result<void> 注销结果.
         */
        Result<void> unregister_handler(virq_t virq) {
            if (!_handlers.contains(virq)) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            _handlers.erase(virq);
            void_return();
        }

        /**
         * @brief 分发一次 virq 到对应处理函数.
         *
         * @param event 待分发中断事件.
         * @return Result<void> 分发结果.
         */
        Result<void> dispatch(const IrqEvent &event) {
            auto resolve_res = resolve(event.virq);
            propagate(resolve_res);

            auto it = _handlers.find(event.virq);
            if (it == _handlers.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            it->second(event);
            void_return();
        }
    };

}  // namespace device
