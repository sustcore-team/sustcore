/**
 * @file platic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 PLATIC 空实现
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/device/eiointc.h>
#include <arch/loongarch64/device/platic.h>
#include <logger.h>
#include <sus/raii.h>

namespace la64 {
    Result<util::owner<PlaticChip *>> PlaticChip::create(
        driver::DriverBase::DevRes res, driver::intc_t identifier,
        driver::intc_t parent_identifier) noexcept {
        if (res.node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *device = new PlaticChip(std::move(res), identifier, parent_identifier);
        if (device == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard device_deleter([device]() { delete device; });

        auto *domain = new driver::LinearIrqDomain<MAX_HW_IRQ>(
            static_cast<driver::domain_t>(identifier), device->name(), *device);
        if (domain == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard domain_deleter([domain]() { delete domain; });

        auto register_res = device->irqman().register_domain(
            util::owner<driver::IrqDomain *>(domain));
        propagate(register_res);
        auto init_res = device->initialize(domain);
        propagate(init_res);

        domain_deleter.release();
        device_deleter.release();
        return util::owner<PlaticChip *>(device);
    }

    PlaticChip::PlaticChip(driver::DriverBase::DevRes res,
                           driver::intc_t identifier,
                           driver::intc_t parent_identifier) noexcept
        : driver::IrqChip(std::move(res)),
          _identifier(identifier),
          _parent_identifier(parent_identifier) {}

    Result<void> PlaticChip::initialize(driver::IrqDomain *domain) noexcept {
        if (domain == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        _domain = domain;
        void_return();
    }

    driver::intc_t PlaticChip::identifier() const noexcept {
        return _identifier;
    }

    Result<void> PlaticChip::enable_irq(driver::hwirq_t hw_irq) noexcept {
        (void)hw_irq;
        void_return();
    }

    Result<void> PlaticChip::disable_irq(driver::hwirq_t hw_irq) noexcept {
        (void)hw_irq;
        void_return();
    }

    Result<void> PlaticChip::set_priority(driver::hwirq_t hw_irq,
                                          driver::domain_t prio) noexcept {
        (void)hw_irq;
        (void)prio;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> PlaticChip::set_affinity(driver::hwirq_t hw_irq,
                                          driver::cpu_mask_t mask) noexcept {
        (void)hw_irq;
        (void)mask;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> PlaticChip::ack(const driver::IrqEvent &event) noexcept {
        (void)event;
        void_return();
    }

    Result<void> PlaticChip::set_trigger(driver::hwirq_t hw_irq,
                                         driver::IrqTrigger trigger) noexcept {
        (void)hw_irq;
        (void)trigger;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> PlaticChip::attach_to_parent_domain(
        driver::IrqManager &irqman,
        driver::IrqDomain &self_domain) noexcept {
        auto parent_domain_res =
            irqman.get_domain(static_cast<driver::domain_t>(_parent_identifier));
        propagate(parent_domain_res);

        for (driver::hwirq_t hw_irq = 0; hw_irq < MAX_HW_IRQ; ++hw_irq) {
            auto parent_virq_res = irqman.allocate_virq(
                parent_domain_res.value().get().id(), hw_irq);
            propagate(parent_virq_res);
            auto register_res = irqman.register_handler(
                parent_virq_res.value(),
                [hw_irq, &self_domain, &irqman](const driver::IrqEvent &event)
                    -> Result<void> {
                    (void)event;
                    auto virq_res = self_domain.to_virq(hw_irq);
                    propagate(virq_res);
                    return irqman.dispatch(driver::IrqEvent{
                        .virq          = virq_res.value(),
                        .hw_irq        = hw_irq,
                        .domain        = &self_domain,
                        .chip_specific = {},
                    });
                });
            propagate(register_res);
        }

        void_return();
    }
}  // namespace la64
