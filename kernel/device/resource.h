/**
 * @file resource.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备资源
 * @version alpha-1.0.0
 * @date 2026-05-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <fwd.h>
#include <device/device.h>
#include <sus/owner.h>
#include <sustcore/addr.h>

#include <vector>
namespace device {
    /**
     * @brief 统一设备节点导出的单个虚拟中断资源.
     */
    class VIrqResource {
    public:
        [[nodiscard]]
        static util::owner<VIrqResource *> make(driver::virq_t virq) noexcept {
            return util::owner<VIrqResource *>(new VIrqResource(virq));
        }

        /**
         * @brief 获取资源中包含的唯一 virq.
         *
         * @return virq_t virq 值.
         */
        [[nodiscard]]
        driver::virq_t virq() const noexcept {
            return _virq;
        }
        /**
         * @brief 为当前资源对应的 virq 注册处理器.
         *
         * @param handler 该 virq 的处理器.
         * @return Result<void> 注册结果.
         */
        [[nodiscard]]
        Result<void> register_handler(driver::IrqHandler handler) const noexcept;
        /**
         * @brief 注销当前资源对应 virq 的处理器.
         *
         * @return Result<void> 注销结果.
         */
        [[nodiscard]]
        Result<void> unregister_handler() const noexcept;

        [[nodiscard]]
        bool registered() const noexcept {
            return _registered;
        }

        [[nodiscard]]
        Result<void> enable() const noexcept;
        [[nodiscard]]
        Result<void> disable() const noexcept;

        [[nodiscard]]
        Result<void> set_priority(driver::irq_prio_t prio) const noexcept;

    private:
        /**
         * @brief 用给定 virq 构造资源对象.
         *
         * @param virq 统一设备节点解析出的单个 virq.
         */
        explicit VIrqResource(driver::virq_t virq) noexcept
            : _registered(false), _virq(virq) {}

        mutable bool _registered = false;
        driver::virq_t _virq = 0;

        friend class DevResManager;
    };

    /**
     * @brief 统一设备节点导出的单个 MMIO 资源.
     */
    class MMIOResource {
    public:
        [[nodiscard]]
        static util::owner<MMIOResource *> make(PhyArea region) noexcept {
            return util::owner<MMIOResource *>(new MMIOResource(region));
        }

        /**
         * @brief 获取资源中包含的唯一 MMIO 区域.
         *
         * @return const PhyArea& MMIO 区域引用.
         */
        [[nodiscard]]
        const PhyArea &region() const noexcept {
            return _region;
        }

        [[nodiscard]]
        bool mapped() const noexcept {
            return _mapped;
        }
    private:
        /**
         * @brief 用给定 MMIO 区域构造资源对象.
         *
         * @param region 统一设备节点解析出的单个 MMIO 区域.
         */
        explicit MMIOResource(PhyArea region) noexcept
            : _region(region), _mapped(false) {}

        PhyArea _region;
        mutable bool _mapped;

        friend class MMIOManager;
        friend class DevResManager;
    };

    /**
     * @brief 负责维护固定 KVA MMIO 映射的管理器.
     */
    class MMIOManager {
        // 将 mmio_addr 变为 kernel virtual address
        /**
         * @brief 将 MMIO 物理地址映射到固定 KVA 窗口地址.
         *
         * @param mmio_addr MMIO 物理地址.
         * @return VirAddr 固定 KVA 地址.
         */
        [[nodiscard]]
        static VirAddr from_mmio_addr(const PhyAddr &mmio_addr) noexcept {
            return VirAddr(KVA_OFFSET + mmio_addr.arith());
        }

        /**
         * @brief 将 MMIO 物理区间映射到固定 KVA 窗口区间.
         *
         * @param mmio_area MMIO 物理区间.
         * @return VirArea 固定 KVA 区间.
         */
        [[nodiscard]]
        static VirArea from_mmio_area(const PhyArea &mmio_area) noexcept {
            return {from_mmio_addr(mmio_area.begin),
                    from_mmio_addr(mmio_area.end)};
        }

    public:
        /**
         * @brief 获取 MMIO 管理器单例.
         *
         * @return MMIOManager& 单例引用.
         */
        [[nodiscard]]
        static MMIOManager &inst() noexcept;
        /**
         * @brief 初始化 MMIO 管理器.
         */
        static void init();
        /**
         * @brief 判断 MMIO 管理器是否已初始化.
         *
         * @return bool 是否已初始化.
         */
        [[nodiscard]]
        static bool initialized() noexcept;

        /**
         * @brief 在主内核页表中建立指定 MMIO 资源的固定 KVA 映射.
         *
         * @param mmio 单个 MMIO 资源.
         * @return Result<KvaAddr> 映射后的基地址.
         */
        [[nodiscard]]
        Result<KvaAddr> map_to_kernel(const MMIOResource &mmio) noexcept;
        /**
         * @brief 从主内核页表中移除指定 MMIO 资源的固定 KVA 映射.
         *
         * @param mmio 单个 MMIO 资源.
         * @return Result<void> 解除结果.
         */
        [[nodiscard]]
        Result<void> unmap_from_kernel(const MMIOResource &mmio) noexcept;
    };

    /**
     * @brief 负责从统一设备节点提取资源对象的管理器.
     */
    class DevResManager {
    public:
        /**
         * @brief 从统一设备节点提取虚拟中断资源.
         *
         * @param node 统一设备节点.
         * @return std::vector<util::owner<VIrqResource *>> 单 virq 资源列表.
         */
        [[nodiscard]]
        static std::vector<util::owner<VIrqResource *>> get_virq_resource(
            const DeviceNode &node) noexcept;
        /**
         * @brief 从统一设备节点提取 MMIO 资源.
         *
         * @param node 统一设备节点.
         * @return std::vector<util::owner<MMIOResource *>> 单区域资源列表.
         */
        [[nodiscard]]
        static std::vector<util::owner<MMIOResource *>> get_mmio_resource(
            const DeviceNode &node) noexcept;
    };
}  // namespace device
