/**
 * @file riscv_intc.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CPU 本地中断设备与驱动
 * @version alpha-1.0.0
 * @date 2026-05-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/csr.h>
#endif
#include <driver/int/riscv_intc.h>
#include <logger.h>
#include <sus/raii.h>

namespace driver {
    namespace {
        constexpr size_t RISCV_INTC_MAX_HW_IRQ = 16;
    }  // namespace

    /**
     * @brief 创建一个 CPU 本地中断设备驱动.
     */
    Result<util::owner<RiscVIntC *>> RiscVIntC::create(
        DevRes res, intc_t identifier, device::cpuid_t hart_id) noexcept {
        if (res.node == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC 创建失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *device = new RiscVIntC(std::move(res), identifier, hart_id);
        if (device == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard device_deleter([device]() { delete device; });

        auto *domain = new LinearIrqDomain<RISCV_INTC_MAX_HW_IRQ>(
            static_cast<domain_t>(identifier), device->name(), *device);
        if (domain == nullptr) {
            loggers::INTERRUPT::ERROR("RiscVIntC[%u] 创建失败: domain 内存不足",
                                      identifier);
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        util::Guard domain_deleter([domain]() { delete domain; });

        auto register_res =
            device->irqman().register_domain(util::owner<IrqDomain *>(domain));
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
        return util::owner<RiscVIntC *>(device);
    }

    /**
     * @brief 构造一个 CPU 本地中断设备驱动.
     */
    RiscVIntC::RiscVIntC(DevRes res, intc_t identifier,
                         device::cpuid_t hart_id) noexcept
        : IrqChip(std::move(res)), _identifier(identifier), _hart_id(hart_id) {}

    /**
     * @brief 获取驱动命中的主 compatible.
     */
    std::string_view RiscVIntC::compatible() const noexcept {
        return COMPATIBLE_STRING;
    }

    /**
     * @brief 初始化 RiscVIntC 的中断域关联.
     */
    Result<void> RiscVIntC::initialize(IrqDomain *domain) {
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

    /**
     * @brief 获取 MMIO 区域列表.
     */
    std::vector<PhyArea> RiscVIntC::mmio_regions() const noexcept {
        std::vector<PhyArea> regions;
        regions.reserve(mmio_resources().size());
        for (const auto &resource : mmio_resources()) {
            assert(resource != nullptr);
            regions.push_back(resource->region());
        }
        return regions;
    }

    /**
     * @brief 获取设备标识.
     */
    intc_t RiscVIntC::identifier() const noexcept {
        return _identifier;
    }

    /**
     * @brief 获取所属 hart.
     */
    device::cpuid_t RiscVIntC::hart_id() const noexcept {
        return _hart_id;
    }

    /**
     * @brief 使能本地中断.
     */
    Result<void> RiscVIntC::enable_irq(hwirq_t hw_irq) noexcept {
        loggers::INTERRUPT::DEBUG("RiscVIntC[%u] enable_irq hwirq=%u",
                                  identifier(), static_cast<unsigned>(hw_irq));
        csr_sie_t sie = csr_get_sie();
        switch (hw_irq) {
            case RiscVIntC::SOFTWARE_LOCAL_IRQ:
                sie.ssie = 1;
                csr_set_sie(sie);
                void_return();
            case RiscVIntC::CLOCK_LOCAL_IRQ:
                sie.stie = 1;
                csr_set_sie(sie);
                void_return();
            case RiscVIntC::EXTERNAL_LOCAL_IRQ:
                sie.seie = 1;
                csr_set_sie(sie);
                void_return();
            default:
                loggers::INTERRUPT::ERROR("RiscVIntC 不支持 enable hwirq=%u",
                                          static_cast<unsigned>(hw_irq));
                unexpect_return(ErrCode::NOT_SUPPORTED);
        }
    }

    /**
     * @brief 屏蔽本地中断.
     */
    Result<void> RiscVIntC::disable_irq(hwirq_t hw_irq) noexcept {
        loggers::INTERRUPT::DEBUG("RiscVIntC[%u] disable_irq hwirq=%u",
                                  identifier(), static_cast<unsigned>(hw_irq));
        csr_sie_t sie = csr_get_sie();
        switch (hw_irq) {
            case RiscVIntC::SOFTWARE_LOCAL_IRQ:
                sie.ssie = 0;
                csr_set_sie(sie);
                void_return();
            case RiscVIntC::CLOCK_LOCAL_IRQ:
                sie.stie = 0;
                csr_set_sie(sie);
                void_return();
            case RiscVIntC::EXTERNAL_LOCAL_IRQ:
                sie.seie = 0;
                csr_set_sie(sie);
                void_return();
            default:
                loggers::INTERRUPT::ERROR("RiscVIntC 不支持 disable hwirq=%u",
                                          static_cast<unsigned>(hw_irq));
                unexpect_return(ErrCode::NOT_SUPPORTED);
        }
    }

    /**
     * @brief 设置中断优先级.
     */
    Result<void> RiscVIntC::set_priority(hwirq_t hw_irq,
                                         domain_t prio) noexcept {
        loggers::INTERRUPT::DEBUG(
            "RiscVIntC[%u] set_priority hwirq=%u prio=%u 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<unsigned>(prio));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    /**
     * @brief 设置中断亲和性.
     */
    Result<void> RiscVIntC::set_affinity(hwirq_t hw_irq,
                                         cpu_mask_t mask) noexcept {
        loggers::INTERRUPT::DEBUG(
            "RiscVIntC[%u] set_affinity hwirq=%u mask=0x%llx 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<unsigned long long>(mask));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    /**
     * @brief 应答中断.
     */
    Result<void> RiscVIntC::ack(const IrqEvent &event) noexcept {
        return enable_irq(event.hw_irq);
    }

    /**
     * @brief 设置中断触发方式.
     */
    Result<void> RiscVIntC::set_trigger(hwirq_t hw_irq,
                                        IrqTrigger trigger) noexcept {
        loggers::INTERRUPT::DEBUG(
            "RiscVIntC[%u] set_trigger hwirq=%u trigger=%d 不支持",
            identifier(), static_cast<unsigned>(hw_irq),
            static_cast<int>(trigger));
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
}  // namespace driver
