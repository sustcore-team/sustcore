/**
 * @file eiointc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 EIOINTC 空实现
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/model.h>

namespace la64 {
    class EioIntcChip final : public driver::IrqChip {
    public:
        static constexpr size_t MAX_HW_IRQ = 32;
        static constexpr const char *COMPATIBLE_STRING =
            "loongson,ls2k2000-eiointc";

        [[nodiscard]]
        static Result<util::owner<EioIntcChip *>> create(
            driver::DriverBase::DevRes res, driver::intc_t identifier,
            driver::hwirq_t parent_hwirq) noexcept;

        ~EioIntcChip() override = default;

        [[nodiscard]]
        driver::intc_t identifier() const noexcept;
        [[nodiscard]]
        driver::hwirq_t parent_hwirq() const noexcept;
        [[nodiscard]]
        Result<void> enable_irq(driver::hwirq_t hw_irq) noexcept override;
        [[nodiscard]]
        Result<void> disable_irq(driver::hwirq_t hw_irq) noexcept override;
        [[nodiscard]]
        Result<void> set_priority(driver::hwirq_t hw_irq,
                                  driver::domain_t prio) noexcept override;
        [[nodiscard]]
        Result<void> set_affinity(driver::hwirq_t hw_irq,
                                  driver::cpu_mask_t mask) noexcept override;
        [[nodiscard]]
        Result<void> ack(const driver::IrqEvent &event) noexcept override;
        [[nodiscard]]
        Result<void> set_trigger(driver::hwirq_t hw_irq,
                                 driver::IrqTrigger trigger) noexcept override;
        [[nodiscard]]
        Result<void> attach_to_parent_domain(
            driver::IrqManager &irqman,
            driver::IrqDomain &self_domain) noexcept override;

    private:
        EioIntcChip(driver::DriverBase::DevRes res, driver::intc_t identifier,
                    driver::hwirq_t parent_hwirq) noexcept;
        [[nodiscard]]
        Result<void> initialize(driver::IrqDomain *domain) noexcept;

        driver::IrqDomain *_domain        = nullptr;
        driver::intc_t _identifier        = device::INVALID_ICTRL_ID;
        driver::hwirq_t _parent_hwirq     = 0;
        driver::virq_t _parent_virq       = 0;

        static driver::IrqManager &irqman() {
            return device::DeviceModel::inst().interrupt();
        }
    };
}  // namespace la64
