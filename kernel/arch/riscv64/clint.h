/**
 * @file clint.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CLINT 设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/int.h>
#include <string>
#include <utility>
#include <vector>

namespace riscv {
    class Clint final : public device::IrqChip {
    public:
        [[nodiscard]]
        static Result<util::owner<Clint *>> create(
            driver::DriverBase::DevRes res, driver::intc_t identifier,
            device::cpuid_t hart_id,
            std::vector<device::cpuid_t> target_harts) noexcept;

        ~Clint() override = default;

        [[nodiscard]]
        std::string_view compatible() const noexcept override;
        [[nodiscard]]
        driver::intc_t identifier() const noexcept;
        [[nodiscard]]
        device::cpuid_t hart_id() const noexcept;
        [[nodiscard]]
        const std::vector<device::cpuid_t> &target_harts() const noexcept;
        [[nodiscard]]
        bool supports_hart(device::cpuid_t hart_id) const noexcept;
        [[nodiscard]]
        driver::virq_t software_virq() const noexcept;
        [[nodiscard]]
        driver::virq_t clock_virq() const noexcept;
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

    private:
        Clint(driver::DriverBase::DevRes res, driver::intc_t identifier,
              device::cpuid_t hart_id,
              std::vector<device::cpuid_t> target_harts) noexcept;

        driver::intc_t _identifier = device::INVALID_ICTRL_ID;
        device::cpuid_t _hart_id = 0;
        std::vector<device::cpuid_t> _target_harts;
    };
}  // namespace riscv
