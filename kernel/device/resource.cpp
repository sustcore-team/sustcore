/**
 * @file resource.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备资源实现
 * @version alpha-1.0.0
 * @date 2026-05-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#include <device/model.h>
#include <device/resource.h>
#include <env.h>
#include <logger.h>

#include <ranges>

namespace {
    device::MMIOManager *g_mmio_manager = nullptr;
    bool g_mmio_manager_initialized     = false;

    /**
     * @brief 将任意 MMIO 区间扩展到页对齐边界.
     *
     * @param area 原始物理区间.
     * @return PhyArea 页对齐后的物理区间.
     */
    [[nodiscard]]
    PhyArea page_align_area(const PhyArea &area) noexcept {
        return PhyArea(area.begin.page_align_down(), area.end.page_align_up());
    }
}  // namespace

namespace device {
    /**
     * @brief 为当前资源对应的 virq 注册处理器.
     */
    Result<void> VIrqResource::register_handler(
        driver::IrqHandler handler) const noexcept {
        if (!DeviceModel::initialized()) {
            loggers::DEVICE::ERROR(
                "注册 virq handler 失败: DeviceModel 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        loggers::DEVICE::DEBUG("注册设备资源 virq handler: virq=%llu",
                               static_cast<unsigned long long>(_virq));
        auto register_res = DeviceModel::inst().interrupt().register_handler(
            _virq, std::move(handler));
        if (register_res.has_value()) {
            _registered = true;
        }
        return register_res;
    }

    /**
     * @brief 注销当前资源对应 virq 的处理器.
     */
    Result<void> VIrqResource::unregister_handler() const noexcept {
        if (!DeviceModel::initialized()) {
            loggers::DEVICE::ERROR(
                "注销 virq handler 失败: DeviceModel 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        loggers::DEVICE::DEBUG("注销设备资源 virq handler: virq=%llu",
                               static_cast<unsigned long long>(_virq));
        auto unregister_res =
            DeviceModel::inst().interrupt().unregister_handler(_virq);
        if (unregister_res.has_value()) {
            _registered = false;
        }
        return unregister_res;
    }

    Result<void> VIrqResource::enable() const noexcept {
        if (!DeviceModel::initialized()) {
            loggers::DEVICE::ERROR("启用 virq 失败: DeviceModel 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        loggers::DEVICE::DEBUG("启用 virq: virq=%llu",
                               static_cast<unsigned long long>(_virq));
        return DeviceModel::inst().interrupt().enable_irq(_virq);
    }
    Result<void> VIrqResource::disable() const noexcept {
        if (!DeviceModel::initialized()) {
            loggers::DEVICE::ERROR("禁用 virq 失败: DeviceModel 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        loggers::DEVICE::DEBUG("禁用 virq: virq=%llu",
                               static_cast<unsigned long long>(_virq));
        return DeviceModel::inst().interrupt().disable_irq(_virq);
    }

    [[nodiscard]]
    Result<void> VIrqResource::set_priority(
        driver::irq_prio_t prio) const noexcept {
        if (!DeviceModel::initialized()) {
            loggers::DEVICE::ERROR(
                "设置 virq 优先级失败: DeviceModel 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        loggers::DEVICE::DEBUG("设置 virq 优先级: virq=%llu, prio=%u",
                               static_cast<unsigned long long>(_virq), prio);
        return DeviceModel::inst().interrupt().set_priority(_virq, prio);
    }

    /**
     * @brief 获取 MMIO 管理器单例.
     */
    MMIOManager &MMIOManager::inst() noexcept {
        assert(g_mmio_manager != nullptr);
        return *g_mmio_manager;
    }

    /**
     * @brief 初始化 MMIO 管理器.
     */
    void MMIOManager::init() {
        if (g_mmio_manager_initialized) {
            return;
        }
        auto *manager = new MMIOManager();
        assert(manager != nullptr);
        g_mmio_manager             = manager;
        g_mmio_manager_initialized = true;
        loggers::DEVICE::INFO("MMIOManager 初始化完成");
    }

    /**
     * @brief 判断 MMIO 管理器是否已初始化.
     */
    bool MMIOManager::initialized() noexcept {
        return g_mmio_manager_initialized;
    }

    /**
     * @brief 在主内核页表中建立指定 MMIO 资源的固定 KVA 映射.
     */
    Result<KvaAddr> MMIOManager::map_to_kernel(
        const MMIOResource &mmio) noexcept {
        if (!initialized()) {
            loggers::DEVICE::ERROR("MMIO 映射失败: MMIOManager 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        if (mmio.region().nullable()) {
            loggers::DEVICE::ERROR("MMIO 映射失败: 目标区间为空");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const PhyArea aligned = page_align_area(mmio.region());
        PageMan kernelman(env::inst().main_kernel_pgd());
        VirAddr kva_start = from_mmio_addr(aligned.begin);
        kernelman.map_range<false>(
            kva_start, aligned.begin, aligned.end - aligned.begin,
            PageMan::page_flags(PageMan::rwx(true, true, false), false, false));
        PageMan::flush_tlb();

        auto mapped = KvaAddr(kva_start.arith());
        loggers::DEVICE::DEBUG("建立 MMIO 映射: pa=[%p,%p) kva=%p",
                               mmio.region().begin.addr(),
                               mmio.region().end.addr(), mapped.addr());
        mmio._mapped = true;
        return mapped;
    }

    /**
     * @brief 从主内核页表中移除指定 MMIO 资源的固定 KVA 映射.
     */
    Result<void> MMIOManager::unmap_from_kernel(
        const MMIOResource &mmio) noexcept {
        if (!initialized()) {
            loggers::DEVICE::ERROR("MMIO 解除映射失败: MMIOManager 未初始化");
            unexpect_return(ErrCode::FAILURE);
        }
        if (mmio.region().nullable()) {
            loggers::DEVICE::ERROR("MMIO 解除映射失败: 目标区间为空");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const VirArea aligned = from_mmio_area(page_align_area(mmio.region()));
        PageMan kernelman(env::inst().main_kernel_pgd());
        kernelman.unmap_range(aligned.begin, aligned.end - aligned.begin);
        PageMan::flush_tlb();

        loggers::DEVICE::DEBUG("解除 MMIO 映射: kva=[%p,%p)",
                               aligned.begin.addr(), aligned.end.addr());
        mmio._mapped = false;
        void_return();
    }

    /**
     * @brief 从统一设备节点提取虚拟中断资源列表.
     */
    std::vector<util::owner<VIrqResource *>> DevResManager::get_virq_resource(
        const DeviceNode &node) noexcept {
        auto virqs = node.irqs();
        if (!virqs.has_value()) {
            loggers::DEVICE::DEBUG("设备节点 %s 未导出 virq 资源",
                                   node.platform());
            return {};
        }
        loggers::DEVICE::DEBUG("提取 virq 资源: count=%u",
                               static_cast<unsigned>(virqs->size()));
        std::vector<util::owner<VIrqResource *>> resources;
        resources.reserve(virqs->size());
        for (auto virq : virqs.value()) {
            loggers::DEVICE::DEBUG("提取单 virq 资源: virq=%llu",
                                   static_cast<unsigned long long>(virq));
            resources.push_back(
                util::owner<VIrqResource *>(new VIrqResource(virq)));
        }
        return resources;
    }

    /**
     * @brief 从统一设备节点提取 MMIO 资源列表.
     */
    std::vector<util::owner<MMIOResource *>> DevResManager::get_mmio_resource(
        const DeviceNode &node) noexcept {
        auto mmio = node.mmio_regions();
        if (!mmio.has_value()) {
            loggers::DEVICE::DEBUG("设备节点 %s 未导出 MMIO 资源",
                                   node.platform());
            return {};
        }
        loggers::DEVICE::DEBUG("提取 MMIO 资源: count=%u",
                               static_cast<unsigned>(mmio->size()));
        std::vector<util::owner<MMIOResource *>> resources;
        resources.reserve(mmio->size());
        for (const auto &region : mmio.value()) {
            loggers::DEVICE::DEBUG("提取单 MMIO 资源: [%p,%p)",
                                   region.begin.addr(), region.end.addr());
            resources.push_back(
                util::owner<MMIOResource *>(new MMIOResource(region)));
        }
        return resources;
    }
}  // namespace device
