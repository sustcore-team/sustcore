/**
 * @file model.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备模型 (ACPI/DTB 的抽象表示)
 * @version alpha-1.0.0
 * @date 2026-05-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/cpu.h>
#include <device/device.h>
#include <device/platform.h>
#include <driver/base.h>
#include <driver/factory.h>
#include <driver/int/base.h>
#include <driver/model.h>
#include <logger.h>
#include <sus/owner.h>
#include <sustcore/addr.h>
#include <sustcore/boot.h>
#include <sustcore/errcode.h>

#include <vector>

namespace device {
    class DeviceModel;
}

namespace device {
    class DeviceProvider {
    public:
        virtual ~DeviceProvider() = default;

        /**
         * @brief 向设备模型注册当前提供者暴露的全部设备信息.
         *
         * @param model 目标设备模型.
         */
        virtual void register_device(DeviceModel &model) const = 0;

        [[nodiscard]]
        virtual const char *name() const = 0;
    };

    class DeviceModel {
    public:
        [[nodiscard]]
        std::vector<MemRegion> memory_regions() {
            return _regions;
        }

        [[nodiscard]]
        CpuGroupInfo &cpus() {
            return _cpus;
        }

        [[nodiscard]]
        Platform *platform() const noexcept {
            return _platform.get();
        }

        [[nodiscard]]
        driver::IrqManager &interrupt() {
            return _interrupt;
        }

        /**
         * @brief 获取已登记的统一设备节点列表.
         *
         * @return const std::vector<util::owner<DeviceNode *>>& 节点 owner
         * 列表.
         */
        [[nodiscard]]
        const std::vector<util::owner<DeviceNode *>> &device_nodes()
            const noexcept {
            return _devices;
        }

        /**
         * @brief 获取全部非中断控制器设备节点.
         *
         * 这些节点仅以非拥有指针形式暴露, 生命周期仍由 DeviceModel 管理.
         *
         * @return const std::vector<DeviceNode *>& 节点列表.
         */
        [[nodiscard]]
        const std::vector<DeviceNode *> &non_irq_device_nodes() const noexcept {
            return _non_irq_devices;
        }

        /**
         * @brief 登记一个统一设备节点并转移所有权给 DeviceModel.
         *
         * @param node 待登记节点所有权.
         * @return Result<DeviceNode*> 已登记节点的非拥有指针.
         */
        [[nodiscard]]
        Result<DeviceNode *> register_device_node(
            util::owner<DeviceNode *> node) noexcept;

        /**
         * @brief 按 compatible 查询全部匹配的设备节点.
         *
         * @param compatible 目标 compatible 字符串.
         * @return std::vector<DeviceNode*> 全部匹配节点的非拥有指针列表.
         */
        [[nodiscard]]
        std::vector<DeviceNode *> find_devices_by_compatible(
            std::string_view compatible) const noexcept;

        /**
         * @brief 将一批内存区域并入设备模型.
         *
         * @param regs 待加入的内存区域集合.
         */
        void collect_memory_regions(std::vector<MemRegion> *regs) {
            if (regs == nullptr) {
                return;
            }
            _regions.insert(_regions.end(), regs->begin(), regs->end());
            _regions = _normalize_memory_regions(_regions);
        }

        /**
         * @brief 回写当前系统的 clock virq.
         *
         * @param virq 时钟中断对应的全局 virq.
         */
        void set_clock_virq(driver::virq_t virq) {
            _clock_virq = virq;
        }

        void set_platform(util::owner<Platform *> platform) noexcept {
            if (_platform.get() != nullptr) {
                delete _platform.get();
            }
            _platform = std::move(platform);
        }

        /**
         * @brief 获取当前系统的 clock virq.
         *
         * @return virq_t clock virq.
         */
        [[nodiscard]]
        driver::virq_t clock_virq() const {
            return _clock_virq;
        }

        void register_provider(util::owner<DeviceProvider *> provider) {
            _providers.push_back(std::move(provider));
            provider->register_device(*this);
            loggers::DEVICE::INFO("已注册设备提供者: %s", provider->name());
        }

        DeviceModel(const DeviceModel &)            = delete;
        DeviceModel &operator=(const DeviceModel &) = delete;
        DeviceModel(DeviceModel &&other)            = delete;
        DeviceModel &operator=(DeviceModel &&other) = delete;

        ~DeviceModel() {
            cleanup();
        }
        void cleanup() {
            if (driver::DriverModel::initialized()) {
                driver::DriverModel::inst().cleanup();
            }
            for (auto &provider : _providers) {
                delete provider.get();
            }
            _providers.clear();
            cleanup_device_nodes();
            if (_platform.get() != nullptr) {
                delete _platform.get();
                _platform = util::owner<Platform *>(nullptr);
            }
        }

        static DeviceModel &inst();
        static void init();
        static bool initialized();

    protected:
        /**
         * @brief 规范化内存区域列表
         *
         * 该函数接受一个可能包含重叠或未排序的内存区域列表, 并返回一个新的列表,
         * 其会依次执行:
         * 1. 将重叠的同状态区域合并为单个连续区域
         * 2. 从 FREE 状态中剔除任何与非 FREE 区域重叠的部分
         * 3. 如果有两个非 FREE 内存区域重叠, 则将重叠部分设置为 BAD_MEMORY 状态
         * 4. 剔除所有空区域 (大小为 0 的区域, 例如上述结果中的 [0x4000, 0x3000)
         * 就是一个空区域)
         * 5. 将区域按起始地址排序
         *
         * @param regions 待规范化的内存区域列表
         * @return std::vector<MemRegion> 规范化后的内存区域列表
         */
        [[nodiscard]]
        std::vector<MemRegion> _normalize_memory_regions(
            const std::vector<MemRegion> &regions) const;

        /**
         * @brief 释放已登记的统一设备节点.
         */
        void cleanup_device_nodes() noexcept;

    private:
        static DeviceModel _INSTANCE;
        static bool _initialized;
        DeviceModel() = default;
        std::vector<util::owner<DeviceProvider *>> _providers;
        std::vector<util::owner<DeviceNode *>> _devices;
        std::vector<DeviceNode *> _non_irq_devices;
        std::vector<MemRegion> _regions;
        CpuGroupInfo _cpus;
        util::owner<Platform *> _platform =
            util::owner<Platform *>(nullptr);
        driver::IrqManager _interrupt;
        driver::virq_t _clock_virq = 0;
    };

    class KernelProvider : public DeviceProvider {
    public:
        void register_device(DeviceModel &model) const override;
        [[nodiscard]]
        const char *name() const override {
            return "kernel";
        }
    };
};  // namespace device
