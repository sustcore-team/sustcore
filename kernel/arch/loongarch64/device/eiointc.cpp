/**
 * @file eiointc.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 EIOINTC 空实现
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/device/eiointc.h>
#include <arch/loongarch64/device/platform.h>
#include <logger.h>
#include <sus/raii.h>

namespace la64 {
    Result<util::owner<EioIntcChip *>> EioIntcChip::create(
        driver::DriverBase::DevRes res, driver::intc_t identifier,
        driver::hwirq_t parent_hwirq) noexcept {
        if (res.node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *device = new EioIntcChip(std::move(res), identifier, parent_hwirq);
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
        return util::owner<EioIntcChip *>(device);
    }

    EioIntcChip::EioIntcChip(driver::DriverBase::DevRes res,
                             driver::intc_t identifier,
                             driver::hwirq_t parent_hwirq) noexcept
        : driver::IrqChip(std::move(res)),
          _identifier(identifier),
          _parent_hwirq(parent_hwirq) {}

    Result<void> EioIntcChip::initialize(driver::IrqDomain *domain) noexcept {
        if (domain == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        _domain = domain;
        void_return();
    }

    driver::intc_t EioIntcChip::identifier() const noexcept {
        return _identifier;
    }

    driver::hwirq_t EioIntcChip::parent_hwirq() const noexcept {
        return _parent_hwirq;
    }

    Result<void> EioIntcChip::enable_irq(driver::hwirq_t hw_irq) noexcept {
        (void)hw_irq;
        void_return();
    }

    Result<void> EioIntcChip::disable_irq(driver::hwirq_t hw_irq) noexcept {
        (void)hw_irq;
        void_return();
    }

    Result<void> EioIntcChip::set_priority(driver::hwirq_t hw_irq,
                                           driver::domain_t prio) noexcept {
        (void)hw_irq;
        (void)prio;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> EioIntcChip::set_affinity(driver::hwirq_t hw_irq,
                                           driver::cpu_mask_t mask) noexcept {
        (void)hw_irq;
        (void)mask;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> EioIntcChip::ack(const driver::IrqEvent &event) noexcept {
        (void)event;
        void_return();
    }

    Result<void> EioIntcChip::set_trigger(driver::hwirq_t hw_irq,
                                          driver::IrqTrigger trigger) noexcept {
        (void)hw_irq;
        (void)trigger;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> EioIntcChip::attach_to_parent_domain(
        driver::IrqManager &irqman,
        driver::IrqDomain &self_domain) noexcept {
        auto parent_domain_res = irqman.get_domain(
            device::DeviceModel::inst().platform()
                ->as<la64::LoongArch64Platform>()
                ->global_intc());
        propagate(parent_domain_res);

        auto parent_virq_res = irqman.allocate_virq(parent_domain_res.value().get().id(),
                                                    _parent_hwirq);
        propagate(parent_virq_res);
        _parent_virq = parent_virq_res.value();

        auto register_res = irqman.register_handler(
            _parent_virq,
            [this, &self_domain, &irqman](const driver::IrqEvent &event) -> Result<void> {
                (void)event;
                if (_domain == nullptr) {
                    unexpect_return(ErrCode::NULLPTR);
                }
                auto virq_res = self_domain.to_virq(0);
                propagate(virq_res);
                return irqman.dispatch(driver::IrqEvent{
                    .virq          = virq_res.value(),
                    .hw_irq        = 0,
                    .domain        = &self_domain,
                    .chip_specific = {},
                });
            });
        propagate(register_res);
        void_return();
    }
}  // namespace la64
