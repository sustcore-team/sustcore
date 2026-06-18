/**
 * @file cpuic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 CPU 本地中断设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/cpuic.h>
#include <arch/loongarch64/csr.h>
#include <logger.h>
#include <sus/raii.h>

namespace la64 {
    Result<util::owner<CpuICChip *>> CpuICChip::create(
        driver::DriverBase::DevRes res, driver::intc_t identifier,
        device::cpuid_t hart_id) noexcept {
        if (res.node == nullptr) {
            loggers::INTERRUPT::ERROR("CpuICChip 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *device = new CpuICChip(std::move(res), identifier, hart_id);
        if (device == nullptr) {
            loggers::INTERRUPT::ERROR("CpuICChip 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard device_deleter([device]() { delete device; });

        auto *domain = new driver::LinearIrqDomain<MAX_HW_IRQ>(
            static_cast<driver::domain_t>(identifier), device->name(), *device);
        if (domain == nullptr) {
            loggers::INTERRUPT::ERROR("CpuICChip[%u] 创建失败: domain 内存不足",
                                      identifier);
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard domain_deleter([domain]() { delete domain; });

        auto register_res = device->irqman().register_domain(
            util::owner<driver::IrqDomain *>(domain));
        if (!register_res.has_value()) {
            loggers::INTERRUPT::ERROR(
                "CpuICChip[%u] 创建失败: 注册 domain 失败 err=%s", identifier,
                to_cstring(register_res.error()));
            propagate_return(register_res);
        }

        auto init_res = device->initialize(domain);
        if (!init_res.has_value()) {
            loggers::INTERRUPT::ERROR("CpuICChip[%u] 初始化失败: err=%s",
                                      identifier, to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        domain_deleter.release();
        device_deleter.release();
        loggers::INTERRUPT::INFO("CpuICChip[%u] 创建成功: hart=%u name=%s",
                                 identifier, hart_id, device->name());
        return util::owner<CpuICChip *>(device);
    }

    CpuICChip::CpuICChip(driver::DriverBase::DevRes res,
                         driver::intc_t identifier,
                         device::cpuid_t hart_id) noexcept
        : driver::IrqChip(std::move(res)),
          _identifier(identifier),
          _hart_id(hart_id) {}

    std::string_view CpuICChip::compatible() const noexcept {
        return COMPATIBLE_STRING;
    }

    driver::intc_t CpuICChip::identifier() const noexcept {
        return _identifier;
    }

    device::cpuid_t CpuICChip::hart_id() const noexcept {
        return _hart_id;
    }

    Result<void> CpuICChip::initialize(driver::IrqDomain *domain) noexcept {
        if (domain == nullptr) {
            loggers::INTERRUPT::ERROR("CpuICChip[%u] 初始化失败: domain 为空",
                                      identifier());
            unexpect_return(ErrCode::NULLPTR);
        }
        _domain = domain;
        loggers::INTERRUPT::DEBUG(
            "CpuICChip[%u] 初始化成功: domain=%u hart=%u", identifier(),
            domain->id(), hart_id());
        void_return();
    }

    bool CpuICChip::valid_hwirq(driver::hwirq_t hw_irq) noexcept {
        return hw_irq < MAX_HW_IRQ;
    }

    Result<void> CpuICChip::set_irq_enabled(driver::hwirq_t hw_irq,
                                            bool enabled) noexcept {
        if (!valid_hwirq(hw_irq)) {
            loggers::INTERRUPT::ERROR("CpuICChip 不支持 hwirq=%u",
                                      static_cast<unsigned>(hw_irq));
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        csr_ecfg_t ecfg = csr_get_ecfg();
        if (enabled) {
            ecfg.lie |= (1ULL << hw_irq);
        } else {
            ecfg.lie &= ~(1ULL << hw_irq);
        }
        csr_set_ecfg(ecfg);
        void_return();
    }

    Result<void> CpuICChip::enable_irq(driver::hwirq_t hw_irq) noexcept {
        return set_irq_enabled(hw_irq, true);
    }

    Result<void> CpuICChip::disable_irq(driver::hwirq_t hw_irq) noexcept {
        return set_irq_enabled(hw_irq, false);
    }

    Result<void> CpuICChip::set_priority(driver::hwirq_t hw_irq,
                                         driver::domain_t prio) noexcept {
        loggers::INTERRUPT::DEBUG(
            "CpuICChip[%u] set_priority hwirq=%u prio=%u 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<unsigned>(prio));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> CpuICChip::set_affinity(driver::hwirq_t hw_irq,
                                         driver::cpu_mask_t mask) noexcept {
        loggers::INTERRUPT::DEBUG(
            "CpuICChip[%u] set_affinity hwirq=%u mask=0x%llx 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<unsigned long long>(mask));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> CpuICChip::ack(const driver::IrqEvent &event) noexcept {
        if (event.hw_irq == TIMER_IRQ) {
            csr_ticlr_t ticlr{};
            ticlr.clr = 1;
            csr_set_ticlr(ticlr);
        }
        return enable_irq(event.hw_irq);
    }

    Result<void> CpuICChip::set_trigger(driver::hwirq_t hw_irq,
                                        driver::IrqTrigger trigger) noexcept {
        loggers::INTERRUPT::DEBUG(
            "CpuICChip[%u] set_trigger hwirq=%u trigger=%d 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<int>(trigger));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> CpuICChip::post_timer() noexcept {
        return post(TIMER_IRQ);
    }

    Result<void> CpuICChip::post_pmc() noexcept {
        return post(PMC_IRQ);
    }

    Result<void> CpuICChip::post_hw(int i) noexcept {
        if (i < 0 || i > 7) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        return post(static_cast<driver::hwirq_t>(HWI_BEGIN + i));
    }

    Result<void> CpuICChip::post(driver::hwirq_t hw_irq) noexcept {
        if (_domain == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto virq_res = _domain->to_virq(hw_irq);
        propagate(virq_res);

        auto disable_res = disable_irq(hw_irq);
        propagate(disable_res);

        return irqman().dispatch(driver::IrqEvent{
            .virq          = virq_res.value(),
            .hw_irq        = hw_irq,
            .domain        = _domain,
            .chip_specific = {},
        });
    }
}  // namespace la64
