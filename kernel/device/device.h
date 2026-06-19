/**
 * @file device.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备抽象
 * @version alpha-1.0.0
 * @date 2026-05-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <fwd.h>
#include <sus/rtti.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

#include <optional>
#include <vector>

namespace driver {
    using domain_t                              = b32;
    using irq_prio_t                            = b32;
    using cpu_mask_t                            = b64;
    using virq_t                                = b64;
    using hwirq_t                               = b32;
    using intc_t                                = b32;
    inline constexpr intc_t INVALID_ICTRL_ID    = 0xFFFFFFFF;
    inline constexpr domain_t INVALID_DOMAIN_ID = 0xFFFFFFFF;
    inline constexpr virq_t INVALID_VIRQ        = 0xFFFFFFFF'FFFFFFFFuLL;

    using IrqHandler = std::function<void(const IrqEvent &)>;
}  // namespace driver

namespace device {
    using driver::cpu_mask_t;
    using driver::domain_t;
    using driver::hwirq_t;
    using driver::intc_t;
    using driver::irq_prio_t;
    using driver::virq_t;
    inline constexpr intc_t INVALID_ICTRL_ID    = driver::INVALID_ICTRL_ID;
    inline constexpr domain_t INVALID_DOMAIN_ID = driver::INVALID_DOMAIN_ID;
    using cpuid_t                               = b32;
    using topo_t                                = b32;

    enum class DevicePlatform {
        FDT,
        PCI,
    };

    /**
     * @brief 设备节点导出的原始中断描述.
     *
     * 中断描述只保存可以由设备模型静态推导出的信息. 其后续是否编译为
     * `virq` 以及采用何种全局编号, 由 `DevResManager` 和 `IrqManager`
     * 统一决定.
     */
    struct RawIrqSpec {
        driver::domain_t domain = driver::INVALID_DOMAIN_ID;
        driver::hwirq_t hwirq = 0;
        std::optional<driver::IrqTrigger> trigger = std::nullopt;
    };

    /**
     * @brief 统一设备节点语义接口.
     *
     * 平台后端应将原始设备描述翻译为“身份 + 资源”的统一节点，而非暴露任意
     * 属性字典。更具体的平台专属访问接口应留在各自后端类型中。
     */
    class DeviceNode : public RTTIBase<DeviceNode, DevicePlatform> {
    public:
        [[nodiscard]]
        virtual DevicePlatform type_id() const = 0;
        virtual ~DeviceNode() = default;

        /**
         * @brief 获取统一设备节点名称.
         *
         * @return const char* 设备名称字符串.
         */
        [[nodiscard]]
        virtual const char *name() const noexcept = 0;

        /**
         * @brief 获取该设备节点的身份平台标识.
         *
         * @return DevicePlatform 平台/身份类型.
         */
        [[nodiscard]]
        DevicePlatform platform() const noexcept {
            return type_id();
        }

        /**
         * @brief 获取设备节点声明的 MMIO 区域列表.
         *
         * @return std::vector<PhyArea> MMIO 区域集合.
         */
        [[nodiscard]]
        virtual std::vector<PhyArea> mmio_regions() const noexcept = 0;

        /**
         * @brief 获取设备节点声明的原始 IRQ 规格列表.
         *
         * @return std::vector<RawIrqSpec> 原始中断描述集合.
         */
        [[nodiscard]]
        virtual std::vector<RawIrqSpec> irq_specs() const noexcept = 0;
    };
}  // namespace device
