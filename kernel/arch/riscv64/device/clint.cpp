/**
 * @file clint.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CLINT 设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/clint.h>
#include <logger.h>

namespace riscv {
    Result<util::owner<Clint *>> Clint::create(
        driver::DriverBase::DevRes res, driver::intc_t identifier,
        device::cpuid_t hart_id,
        std::vector<device::cpuid_t> target_harts) noexcept {
        if (res.node == nullptr) {
            loggers::INTERRUPT::ERROR("Clint 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *device = new Clint(std::move(res), identifier, hart_id,
                                 std::move(target_harts));
        if (device == nullptr) {
            loggers::INTERRUPT::ERROR("Clint 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        if (device->mmio_resources().empty()) {
            loggers::INTERRUPT::ERROR("Clint[%u] 创建失败: 缺少 MMIO 资源",
                                      identifier);
            delete device;
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (device->virq_resources().size() < 2) {
            loggers::INTERRUPT::ERROR(
                "Clint[%u] 创建失败: virq 资源数量不足 count=%u", identifier,
                static_cast<unsigned>(device->virq_resources().size()));
            delete device;
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        loggers::INTERRUPT::DEBUG(
            "创建 Clint 设备: id=%u hart=%u target_harts=%u sw_virq=%llu clock_virq=%llu",
            identifier, hart_id,
            static_cast<unsigned>(device->_target_harts.size()),
            static_cast<unsigned long long>(device->software_virq()),
            static_cast<unsigned long long>(device->clock_virq()));
        return util::owner<Clint *>(device);
    }

    Clint::Clint(driver::DriverBase::DevRes res, driver::intc_t identifier,
                 device::cpuid_t hart_id,
                 std::vector<device::cpuid_t> target_harts) noexcept
        : device::IrqChip(std::move(res)),
          _identifier(identifier),
          _hart_id(hart_id),
          _target_harts(std::move(target_harts)) {}

    driver::intc_t Clint::identifier() const noexcept {
        return _identifier;
    }

    device::cpuid_t Clint::hart_id() const noexcept {
        return _hart_id;
    }

    const std::vector<device::cpuid_t> &Clint::target_harts() const noexcept {
        return _target_harts;
    }

    bool Clint::supports_hart(device::cpuid_t hart_id) const noexcept {
        for (auto target : _target_harts) {
            if (target == hart_id) {
                return true;
            }
        }
        return false;
    }

    driver::virq_t Clint::software_virq() const noexcept {
        assert(virq_resources().size() >= 2);
        assert(virq_resources().at(0) != nullptr);
        return virq_resources().at(0)->virq();
    }

    driver::virq_t Clint::clock_virq() const noexcept {
        assert(virq_resources().size() >= 2);
        assert(virq_resources().at(1) != nullptr);
        return virq_resources().at(1)->virq();
    }

    Result<void> Clint::enable_irq(driver::hwirq_t hw_irq) noexcept {
        loggers::INTERRUPT::ERROR("Clint[%u] 不支持 enable hwirq=%u",
                                  identifier(), hw_irq);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::disable_irq(driver::hwirq_t hw_irq) noexcept {
        loggers::INTERRUPT::ERROR("Clint[%u] 不支持 disable hwirq=%u",
                                  identifier(), hw_irq);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::set_priority(driver::hwirq_t hw_irq,
                                     driver::domain_t prio) noexcept {
        loggers::INTERRUPT::DEBUG(
            "Clint[%u] set_priority hwirq=%u prio=%u 不支持", identifier(),
            hw_irq, prio);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::set_affinity(driver::hwirq_t hw_irq,
                                     driver::cpu_mask_t mask) noexcept {
        loggers::INTERRUPT::ERROR(
            "Clint[%u] set_affinity hwirq=%u mask=0x%llx 不支持",
            identifier(), hw_irq, static_cast<unsigned long long>(mask));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::ack(const driver::IrqEvent &event) noexcept {
        loggers::INTERRUPT::DEBUG("Clint[%u] ack hwirq=%u 不支持",
                                  identifier(), event.hw_irq);
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Clint::set_trigger(driver::hwirq_t hw_irq,
                                    driver::IrqTrigger trigger) noexcept {
        loggers::INTERRUPT::DEBUG(
            "Clint[%u] set_trigger hwirq=%u trigger=%d 不支持", identifier(),
            hw_irq, static_cast<int>(trigger));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
}  // namespace riscv
