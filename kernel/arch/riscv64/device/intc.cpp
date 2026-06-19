/**
 * @file intc.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CPU 本地中断设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/intc.h>
#include <logger.h>
#include <sus/raii.h>

namespace riscv {
    namespace {
        constexpr size_t RISCV_INTC_MAX_HW_IRQ = 16;
    }  // namespace

    Result<util::owner<IntC *>> IntC::create(driver::DriverBase::DevRes res,
                                             driver::intc_t identifier,
                                             device::cpuid_t hart_id) noexcept {
        if (res.node == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *device = new IntC(std::move(res), identifier, hart_id);
        if (device == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard device_deleter([device]() { delete device; });

        auto *domain = new driver::LinearIrqDomain<RISCV_INTC_MAX_HW_IRQ>(
            static_cast<driver::domain_t>(identifier), device->name(), *device);
        if (domain == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC[%u] 创建失败: domain 内存不足",
                                      identifier);
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard domain_deleter([domain]() { delete domain; });

        auto register_res =
            device->irqman().register_domain(util::owner<driver::IrqDomain *>(domain));
        if (!register_res.has_value()) {
            loggers::INTERRUPT::ERROR(
                "RiscVIntC[%u] 创建失败: 注册 domain 失败 err=%s", identifier,
                to_cstring(register_res.error()));
            propagate_return(register_res);
        }

        auto init_res = device->initialize(domain);
        if (!init_res.has_value()) {
            loggers::INTERRUPT::ERROR("RiscVIntC[%u] 初始化失败: err=%s",
                                      identifier, to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        domain_deleter.release();
        device_deleter.release();
        loggers::INTERRUPT::INFO("RiscVIntC[%u] 创建成功: hart=%u name=%s",
                                 identifier, hart_id, device->name());
        return util::owner<IntC *>(device);
    }

    IntC::IntC(driver::DriverBase::DevRes res, driver::intc_t identifier,
               device::cpuid_t hart_id) noexcept
        : driver::IrqChip(std::move(res)),
          _identifier(identifier),
          _hart_id(hart_id) {}

    std::string_view IntC::compatible() const noexcept {
        return COMPATIBLE_STRING;
    }

    Result<void> IntC::initialize(driver::IrqDomain *domain) {
        if (domain == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC[%u] 初始化失败: domain 为空",
                                      identifier());
            unexpect_return(ErrCode::NULLPTR);
        }
        _domain = domain;
        loggers::INTERRUPT::DEBUG("RiscVIntC[%u] 初始化成功: domain=%u hart=%u",
                                  identifier(), domain->id(), hart_id());
        void_return();
    }

    std::vector<PhyArea> IntC::mmio_regions() const noexcept {
        std::vector<PhyArea> regions;
        regions.reserve(mmio_resources().size());
        for (const auto &resource : mmio_resources()) {
            assert(resource != nullptr);
            regions.push_back(resource->region());
        }
        return regions;
    }

    driver::intc_t IntC::identifier() const noexcept {
        return _identifier;
    }

    device::cpuid_t IntC::hart_id() const noexcept {
        return _hart_id;
    }

    Result<void> IntC::enable_irq(driver::hwirq_t hw_irq) noexcept {
        loggers::INTERRUPT::DEBUG("RiscVIntC[%u] enable_irq hwirq=%u",
                                  identifier(), static_cast<unsigned>(hw_irq));
        csr_sie_t sie = csr_get_sie();
        switch (hw_irq) {
        case IntC::SOFTWARE_LOCAL_IRQ:
            sie.ssie = 1;
            csr_set_sie(sie);
            void_return();
        case IntC::CLOCK_LOCAL_IRQ:
            sie.stie = 1;
            csr_set_sie(sie);
            void_return();
        case IntC::EXTERNAL_LOCAL_IRQ:
            sie.seie = 1;
            csr_set_sie(sie);
            void_return();
        default:
            loggers::INTERRUPT::ERROR("RiscVIntC 不支持 enable hwirq=%u",
                                      static_cast<unsigned>(hw_irq));
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
    }

    Result<void> IntC::disable_irq(driver::hwirq_t hw_irq) noexcept {
        loggers::INTERRUPT::DEBUG("RiscVIntC[%u] disable_irq hwirq=%u",
                                  identifier(), static_cast<unsigned>(hw_irq));
        csr_sie_t sie = csr_get_sie();
        switch (hw_irq) {
        case IntC::SOFTWARE_LOCAL_IRQ:
            sie.ssie = 0;
            csr_set_sie(sie);
            void_return();
        case IntC::CLOCK_LOCAL_IRQ:
            sie.stie = 0;
            csr_set_sie(sie);
            void_return();
        case IntC::EXTERNAL_LOCAL_IRQ:
            sie.seie = 0;
            csr_set_sie(sie);
            void_return();
        default:
            loggers::INTERRUPT::ERROR("RiscVIntC 不支持 disable hwirq=%u",
                                      static_cast<unsigned>(hw_irq));
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
    }

    Result<void> IntC::set_priority(driver::hwirq_t hw_irq,
                                    driver::domain_t prio) noexcept {
        loggers::INTERRUPT::DEBUG(
            "RiscVIntC[%u] set_priority hwirq=%u prio=%u 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<unsigned>(prio));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> IntC::set_affinity(driver::hwirq_t hw_irq,
                                    driver::cpu_mask_t mask) noexcept {
        loggers::INTERRUPT::DEBUG(
            "RiscVIntC[%u] set_affinity hwirq=%u mask=0x%llx 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<unsigned long long>(mask));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> IntC::ack(const driver::IrqEvent &event) noexcept {
        return enable_irq(event.hw_irq);
    }

    Result<void> IntC::set_trigger(driver::hwirq_t hw_irq,
                                   driver::IrqTrigger trigger) noexcept {
        loggers::INTERRUPT::DEBUG(
            "RiscVIntC[%u] set_trigger hwirq=%u trigger=%d 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<int>(trigger));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
}  // namespace riscv
