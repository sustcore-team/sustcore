/**
 * @file model.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 驱动模型
 * @version alpha-1.0.0
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/model.h>
#include <driver/model.h>
#include <guard.h>
#include <logger.h>
#include <object/perm.h>
#include <vfs/device.h>
#include <vfs/vfs.h>

namespace driver {
    DriverModel DriverModel::_INSTANCE;
    bool DriverModel::_initialized = false;

    DriverModel &DriverModel::inst() noexcept {
        assert(_initialized);
        return _INSTANCE;
    }

    void DriverModel::init() noexcept {
        new (&_INSTANCE) DriverModel();
        _initialized = true;
    }

    bool DriverModel::initialized() noexcept {
        return _initialized;
    }

    Result<void> DriverModel::activate_runtime(
        const std::vector<device::DeviceNode *> &device_nodes) noexcept {
        _device_nodes = device_nodes;
        auto dir_res  = _register_device_directories();
        propagate(dir_res);
        _runtime_activated = true;
        void_return();
    }

    void DriverModel::cleanup() noexcept {
        _bound_devices.clear();

        for (auto &driver : _drivers) {
            delete driver.get();
        }
        _drivers.clear();

        for (auto &factory : _owned_irq_factories) {
            delete factory.get();
        }
        _owned_irq_factories.clear();

        for (auto &factory : _owned_device_factories) {
            delete factory.get();
        }
        _owned_device_factories.clear();

        for (auto *node : _device_nodes) {
            (void)node;
        }
        _device_nodes.clear();
    }

    Result<void> DriverModel::register_factory(
        util::owner<IDeviceFactory *> factory) noexcept {
        if (factory.get() == nullptr) {
            loggers::DEVICE::ERROR("注册 DeviceFactory 失败: factory 为空");
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *raw          = factory.get();
        auto register_res  = _device_factories.register_factory(*raw);
        propagate(register_res);
        _owned_device_factories.push_back(std::move(factory));
        if (!_runtime_activated) {
            void_return();
        }
        auto bind_res = _probe_new_factory(*raw);
        propagate(bind_res);
        void_return();
    }

    Result<void> DriverModel::register_factory(
        util::owner<IIrqChipFactory *> factory) noexcept {
        if (factory.get() == nullptr) {
            loggers::DEVICE::ERROR("注册 IrqChipFactory 失败: factory 为空");
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *raw = factory.get();
        auto register_res = _irq_factories.register_factory(*raw);
        propagate(register_res);
        _owned_irq_factories.push_back(std::move(factory));
        void_return();
    }

    Result<DriverBase *> DriverModel::create_irq_driver(
        device::DeviceNode *node) noexcept {
        if (node == nullptr) {
            loggers::DEVICE::ERROR("创建 IRQ 驱动失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }
        const IIrqChipFactory *irq_factory = nullptr;
        MatchResult match{};
        for (const auto *factory : _irq_factories.factories()) {
            if (factory == nullptr) {
                continue;
            }
            auto candidate = _irq_factories.match(*factory, *node);
            if (!candidate.matched) {
                continue;
            }
            if (!factory->probe(*node, device::DeviceModel::inst(),
                                candidate.driver_flag))
            {
                continue;
            }
            irq_factory = factory;
            match       = candidate;
            break;
        }
        if (irq_factory == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        loggers::DEVICE::INFO("create_irq_driver: node=%s 开始调用工厂 create",
                              node->name());
        auto driver_res = irq_factory->create(*node, device::DeviceModel::inst(),
                                              match.driver_flag);
        if (!driver_res.has_value()) {
            loggers::DEVICE::ERROR(
                "create_irq_driver: node=%s 工厂 create 失败 err=%s",
                node->name(), to_cstring(driver_res.error()));
        }
        propagate(driver_res);
        if (driver_res.value() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        _drivers.push_back(util::owner<DriverBase *>(driver_res.value()));
        return driver_res.value();
    }

    Result<void> DriverModel::register_runtime_device(
        device::DeviceNode *node) noexcept {
        if (node == nullptr) {
            loggers::DEVICE::ERROR("运行时登记设备失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto it = std::find(_device_nodes.begin(), _device_nodes.end(), node);
        if (it != _device_nodes.end()) {
            loggers::DEVICE::DEBUG("运行时设备已登记: node=%s", node->name());
            void_return();
        }

        _device_nodes.push_back(node);
        if (!_runtime_activated) {
            loggers::DEVICE::DEBUG("运行时设备登记已缓存: node=%s",
                                   node->name());
            void_return();
        }

        auto dir_res = _register_device_directory(*node);
        propagate(dir_res);

        for (const auto *factory : _device_factories.factories()) {
            if (factory == nullptr) {
                continue;
            }

            auto match = _device_factories.match(*factory, *node);
            if (!match.matched) {
                continue;
            }
            if (!factory->probe(*node, device::DeviceModel::inst(),
                                match.driver_flag))
            {
                continue;
            }

            auto bind_res = _bind_device_with_factory(*node, *factory, match);
            if (!bind_res.has_value()) {
                loggers::DEVICE::ERROR(
                    "运行时绑定驱动失败: node=%s err=%s", node->name(),
                    to_cstring(bind_res.error()));
            }
            break;
        }

        loggers::DEVICE::INFO("已接入运行时设备节点: node=%s", node->name());
        void_return();
    }

    Result<void> DriverModel::_register_device_directories() noexcept {
        for (auto *node : _device_nodes) {
            if (node == nullptr) {
                continue;
            }
            auto dir_res = _register_device_directory(*node);
            if (!dir_res.has_value()) {
                loggers::DEVICE::ERROR(
                    "创建 devfs 设备目录失败: node=%s err=%s", node->name(),
                    to_cstring(dir_res.error()));
            }
        }
        void_return();
    }

    Result<void> DriverModel::_register_device_directory(
        device::DeviceNode &node) noexcept {
        auto dup_res = _find_bound_device(node);
        if (dup_res.has_value()) {
            loggers::DEVICE::ERROR("设备目录重复登记: node=%s", node.name());
            unexpect_return(ErrCode::KEY_DUPLICATED);
        }

        auto holder_res = cap::CHolderManager::inst().create_holder();
        propagate(holder_res);
        auto *holder = holder_res.value();
        auto holder_guard = util::Guard([holder]() {
            auto remove_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            if (!remove_res.has_value()) {
                loggers::DEVICE::WARN(
                    "回滚驱动 holder 失败: holder=%u err=%s",
                    static_cast<unsigned>(holder->id()),
                    to_cstring(remove_res.error()));
            }
        });

        auto devdir_res = _open_devdir(node.name(), *holder);
        propagate(devdir_res);

        _bound_devices.push_back(BoundDevice{
            .node       = &node,
            .driver     = nullptr,
            .factory    = nullptr,
            .match_index = -1,
            .driver_flag = 0,
            .devdir     = devdir_res.value(),
            .holder     = util::owner<cap::CHolder *>(holder),
        });
        holder_guard.release();
        void_return();
    }

    Result<void> DriverModel::_probe_new_factory(
        const IDeviceFactory &factory) noexcept {
        const size_t initial_count = _device_nodes.size();
        for (size_t index = 0; index < initial_count; ++index) {
            auto *node = _device_nodes[index];
            if (node == nullptr) {
                continue;
            }

            auto bound_res = _find_bound_device(*node);
            if (!bound_res.has_value()) {
                continue;
            }
            auto *bound = bound_res.value();
            assert(bound != nullptr);

            auto match = _device_factories.match(factory, *node);
            if (!match.matched) {
                continue;
            }

            if (!factory.probe(*node, device::DeviceModel::inst(),
                               match.driver_flag))
            {
                continue;
            }

            if (bound->driver == nullptr) {
                auto bind_res = _bind_device_with_factory(*node, factory, match);
                if (!bind_res.has_value()) {
                    loggers::DEVICE::ERROR("绑定驱动失败: node=%s err=%s",
                                           node->name(),
                                           to_cstring(bind_res.error()));
                }
            }
        }
        void_return();
    }

    Result<void> DriverModel::_bind_device_with_factory(
        device::DeviceNode &node, const IDeviceFactory &factory,
        MatchResult match) noexcept {
        auto bound_res = _find_bound_device(node);
        propagate(bound_res);
        auto *bound = bound_res.value();
        if (bound == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (bound->driver != nullptr) {
            unexpect_return(ErrCode::BUSY);
        }
        if (bound->holder.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto driver_res = _create_driver(node, factory, match.driver_flag);
        propagate(driver_res);
        auto *driver = driver_res.value();
        assert(driver != nullptr);

        driver->bind_holder(*bound->holder.get());
        auto mount_res = driver->mount(bound->devdir);
        if (!mount_res.has_value()) {
            loggers::DEVICE::ERROR("驱动挂载 devfs 失败: node=%s err=%s",
                                   node.name(), to_cstring(mount_res.error()));
            unexpect_return(mount_res.error());
        }

        bound->driver           = driver;
        bound->factory          = &factory;
        bound->match_index      = match.index;
        bound->driver_flag      = match.driver_flag;
        loggers::DEVICE::INFO("已绑定驱动: node=%s match=%d flag=%llu",
                              node.name(), match.index,
                              static_cast<unsigned long long>(match.driver_flag));
        void_return();
    }

    Result<DriverModel::BoundDevice *> DriverModel::_find_bound_device(
        const device::DeviceNode &node) noexcept {
        for (auto &bound : _bound_devices) {
            if (bound.node == &node) {
                return &bound;
            }
        }
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<const DriverModel::BoundDevice *> DriverModel::_find_bound_device(
        const device::DeviceNode &node) const noexcept {
        for (const auto &bound : _bound_devices) {
            if (bound.node == &node) {
                return &bound;
            }
        }
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<DriverBase *> DriverModel::_create_driver(
        device::DeviceNode &node, const IDeviceFactory &factory,
        b64 driver_flag) noexcept {
        auto driver_res =
            factory.create(node, device::DeviceModel::inst(), driver_flag);
        propagate(driver_res);

        if (driver_res.value() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        _drivers.push_back(util::owner<DriverBase *>(driver_res.value()));
        return driver_res.value();
    }

    Result<CapIdx> DriverModel::_open_devdir(const char *name,
                                             cap::CHolder &holder) noexcept {
        auto &vfs = VFS::inst();
        auto devfs_res = vfs.devfs();
        propagate(devfs_res);

        auto root_res = vfs.open_dir(devfs::DEVFS_MOUNT_PATH, holder,
                                     perm::vdir::READ | perm::vdir::WRITE |
                                         perm::vdir::EXEC);
        propagate(root_res);
        auto root_guard = remove_guard(&holder, root_res.value());

        auto root_lookup_res = holder.lookup(root_res.value());
        propagate(root_lookup_res);

        auto dir_res =
            vfs.mkdir(*root_lookup_res.value(), name,
                      flags::O_READ | flags::O_WRITE | flags::O_EXECUTE, holder);
        if (!dir_res.has_value()) {
            if (dir_res.error() == ErrCode::KEY_DUPLICATED) {
                loggers::DEVICE::ERROR("devfs 目录名冲突: %s", name);
            }
            propagate_return(dir_res);
        }

        root_guard.release();
        return dir_res.value();
    }

}  // namespace driver
