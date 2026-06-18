/**
 * @file intc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CPU 本地中断设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/model.h>

#include <string>

namespace riscv {
    class IntC final : public driver::IrqChip {
    public:
        static constexpr driver::hwirq_t SOFTWARE_LOCAL_IRQ_S  = 1;
        static constexpr driver::hwirq_t SOFTWARE_LOCAL_IRQ    = 3;
        static constexpr driver::hwirq_t CLOCK_LOCAL_IRQ_S     = 5;
        static constexpr driver::hwirq_t CLOCK_LOCAL_IRQ       = 7;
        static constexpr driver::hwirq_t EXTERNAL_LOCAL_IRQ    = 9;
        static constexpr const char *COMPATIBLE_STRING         = "riscv,local-intc";

        [[nodiscard]]
        static Result<util::owner<IntC *>> create(
            driver::DriverBase::DevRes res, driver::intc_t identifier,
            device::cpuid_t hart_id) noexcept;

        ~IntC() override = default;

        [[nodiscard]]
        std::string_view compatible() const noexcept override;

        [[nodiscard]]
        std::vector<PhyArea> mmio_regions() const noexcept;

        [[nodiscard]]
        driver::intc_t identifier() const noexcept;

        [[nodiscard]]
        device::cpuid_t hart_id() const noexcept;

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
        Result<void> post_timer() noexcept {
            return post(CLOCK_LOCAL_IRQ);
        }

        [[nodiscard]]
        Result<void> post_external() noexcept {
            return post(EXTERNAL_LOCAL_IRQ);
        }

        [[nodiscard]]
        Result<void> post(driver::hwirq_t hw_irq) noexcept {
            auto virq_res = _domain->to_virq(hw_irq);
            propagate(virq_res);

            auto res = disable_irq(hw_irq);
            propagate(res);

            return irqman().dispatch(driver::IrqEvent{
                .virq          = virq_res.value(),
                .hw_irq        = hw_irq,
                .domain        = _domain,
                .chip_specific = {},
            });
        }

    private:
        driver::IrqManager &irqman() {
            return device::DeviceModel::inst().interrupt();
        }

        IntC(driver::DriverBase::DevRes res, driver::intc_t identifier,
             device::cpuid_t hart_id) noexcept;

        [[nodiscard]]
        Result<void> initialize(driver::IrqDomain *domain);

        driver::IrqDomain *_domain     = nullptr;
        driver::intc_t _identifier     = device::INVALID_ICTRL_ID;
        device::cpuid_t _hart_id       = 0;
    };
}  // namespace riscv
