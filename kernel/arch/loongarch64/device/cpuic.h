/**
 * @file cpuic.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 CPU 本地中断设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/model.h>

#include <string>

namespace la64 {
    class CpuICChip final : public driver::IrqChip {
    public:
        static constexpr driver::hwirq_t HWI_BEGIN        = 2;
        static constexpr driver::hwirq_t HWI_END          = 9;
        static constexpr driver::hwirq_t TIMER_IRQ        = 11;
        static constexpr driver::hwirq_t PMC_IRQ          = 12;
        static constexpr size_t MAX_HW_IRQ                = 13;
        static constexpr const char *COMPATIBLE_STRING =
            "loongson,cpu-interrupt-controller";

        [[nodiscard]]
        static Result<util::owner<CpuICChip *>> create(
            driver::DriverBase::DevRes res, driver::intc_t identifier,
            device::cpuid_t hart_id) noexcept;

        ~CpuICChip() override = default;

        [[nodiscard]]
        std::string_view compatible() const noexcept override;

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
        Result<void> post_timer() noexcept;

        [[nodiscard]]
        Result<void> post_pmc() noexcept;

        [[nodiscard]]
        Result<void> post_hw(int i) noexcept;

        [[nodiscard]]
        Result<void> post(driver::hwirq_t hw_irq) noexcept;

    private:
        [[nodiscard]]
        Result<void> initialize(driver::IrqDomain *domain) noexcept;

        [[nodiscard]]
        static bool valid_hwirq(driver::hwirq_t hw_irq) noexcept;

        [[nodiscard]]
        Result<void> set_irq_enabled(driver::hwirq_t hw_irq,
                                     bool enabled) noexcept;

        driver::IrqManager &irqman() {
            return device::DeviceModel::inst().interrupt();
        }

        CpuICChip(driver::DriverBase::DevRes res, driver::intc_t identifier,
                  device::cpuid_t hart_id) noexcept;

        driver::IrqDomain *_domain = nullptr;
        driver::intc_t _identifier = device::INVALID_ICTRL_ID;
        device::cpuid_t _hart_id   = 0;
    };
}  // namespace la64
