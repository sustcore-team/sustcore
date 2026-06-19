/**
 * @file model.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 驱动模型
 * @version alpha-1.0.0
 * @date 2026-05-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <fwd.h>
#include <cap/cholder.h>
#include <device/device.h>
#include <driver/base.h>
#include <driver/factory.h>
#include <sus/owner.h>

#include <vector>

namespace driver {
    /**
     * @brief 驱动模型.
     *
     * 统一掌管驱动工厂注册、工厂对象生命周期和运行时驱动对象生命周期.
     */
    class DriverModel {
    public:
        DriverModel(const DriverModel &)            = delete;
        DriverModel &operator=(const DriverModel &) = delete;
        DriverModel(DriverModel &&)                 = delete;
        DriverModel &operator=(DriverModel &&)      = delete;

        /**
         * @brief 销毁驱动模型.
         */
        ~DriverModel() {
            cleanup();
        }

        /**
         * @brief 获取驱动模型单例.
         *
         * @return DriverModel& 单例引用.
         */
        [[nodiscard]]
        static DriverModel &inst() noexcept;

        /**
         * @brief 初始化驱动模型单例.
         */
        static void init() noexcept;

        /**
         * @brief 判断驱动模型是否已初始化.
         *
         * @return bool 是否已初始化.
         */
        [[nodiscard]]
        static bool initialized() noexcept;

        /**
         * @brief 在 VFS/devfs 就绪后接入普通设备节点并创建设备目录.
         *
         * @param device_nodes 非中断控制器设备节点列表.
         * @return Result<void> 接入结果.
         */
        [[nodiscard]]
        Result<void> activate_runtime(
            const std::vector<device::DeviceNode *> &device_nodes) noexcept;

        /**
         * @brief 清理全部工厂对象与运行时驱动对象.
         */
        void cleanup() noexcept;

        /**
         * @brief 注册普通设备工厂并接管其所有权.
         *
         * @param factory 工厂对象 owner.
         * @return Result<void> 注册结果.
         */
        [[nodiscard]]
        Result<void> register_factory(
            util::owner<IDeviceFactory *> factory) noexcept;

        /**
         * @brief 注册 IRQ 设备工厂并接管其所有权.
         *
         * @param factory 工厂对象 owner.
         * @return Result<void> 注册结果.
         */
        [[nodiscard]]
        Result<void> register_factory(
            util::owner<IIrqChipFactory *> factory) noexcept;

        /**
         * @brief 基于指定 IRQ 设备节点创建匹配驱动并接管其生命周期.
         *
         * 仅供早期 IRQ 控制器初始化路径使用.
         *
         * @param node 统一设备节点非拥有指针.
         * @return Result<DriverBase*> 创建成功的驱动非拥有指针.
         */
        [[nodiscard]]
        Result<DriverBase *> create_irq_driver(device::DeviceNode *node) noexcept;

        /**
         * @brief 在运行时接入一个新登记的统一设备节点.
         *
         * 供晚注册的 DeviceProvider 使用，使新增节点进入 devfs 与后续驱动
         * 匹配视图。
         *
         * @param node 新设备节点非拥有指针.
         * @return Result<void> 接入结果.
         */
        [[nodiscard]]
        Result<void> register_runtime_device(device::DeviceNode *node) noexcept;

        /**
         * @brief 获取普通设备工厂注册表只读引用.
         *
         * @return const DeviceFactoryRegistry& 普通工厂注册表.
         */
        [[nodiscard]]
        const DeviceFactoryRegistry &device_factories() const noexcept {
            return _device_factories;
        }

        /**
         * @brief 获取 IRQ 工厂注册表只读引用.
         *
         * @return const IrqChipFactoryRegistry& IRQ 工厂注册表.
         */
        [[nodiscard]]
        const IrqChipFactoryRegistry &irq_factories() const noexcept {
            return _irq_factories;
        }

    private:
        struct BoundDevice {
            device::DeviceNode *node          = nullptr;
            DriverBase *driver                = nullptr;
            const IDeviceFactory *factory     = nullptr;
            int match_index                   = -1;
            b64 driver_flag                   = 0;
            CapIdx devdir                     = 0;
            util::owner<cap::CHolder *> holder = util::owner<cap::CHolder *>(
                nullptr);
        };

        DriverModel() = default;

        [[nodiscard]]
        Result<void> _register_device_directories() noexcept;
        [[nodiscard]]
        Result<void> _register_device_directory(device::DeviceNode &node) noexcept;
        [[nodiscard]]
        Result<void> _bind_device_with_factory(device::DeviceNode &node,
                                               const IDeviceFactory &factory,
                                               MatchResult match) noexcept;
        [[nodiscard]]
        Result<void> _probe_new_factory(const IDeviceFactory &factory) noexcept;
        [[nodiscard]]
        Result<BoundDevice *> _find_bound_device(
            const device::DeviceNode &node) noexcept;
        [[nodiscard]]
        Result<const BoundDevice *> _find_bound_device(
            const device::DeviceNode &node) const noexcept;
        [[nodiscard]]
        Result<DriverBase *> _create_driver(device::DeviceNode &node,
                                            const IDeviceFactory &factory,
                                            b64 driver_flag) noexcept;
        [[nodiscard]]
        Result<CapIdx> _open_devdir(const char *name, cap::CHolder &holder) noexcept;
        static DriverModel _INSTANCE;
        static bool _initialized;

        DeviceFactoryRegistry _device_factories;
        IrqChipFactoryRegistry _irq_factories;
        std::vector<util::owner<IDeviceFactory *>> _owned_device_factories;
        std::vector<util::owner<IIrqChipFactory *>> _owned_irq_factories;
        std::vector<device::DeviceNode *> _device_nodes;
        std::vector<BoundDevice> _bound_devices;
        std::vector<util::owner<DriverBase *>> _drivers;
        bool _runtime_activated = false;
    };
}  // namespace driver
