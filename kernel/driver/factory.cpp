/**
 * @file factory.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备工厂注册表
 * @version alpha-1.0.0
 * @date 2026-05-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <driver/factory.h>
#include <logger.h>

#include <device/fdt/device_node.h>
#include <device/model.h>
#include <device/pci.h>

namespace {
    [[nodiscard]]
    bool is_end_id(const driver::PCIDeviceId &id) noexcept {
        return id.vendor_id == 0 && id.subvendor_id == 0 && id.device_id == 0 &&
               id.subdevice_id == 0 && id.class_mask == 0;
    }

    [[nodiscard]]
    driver::MatchResult match_fdt_ids(const driver::FDTDeviceId *ids,
                                      const device::DeviceNode &node) noexcept {
        driver::MatchResult result{};
        auto *fdt_node = node.as<fdt::FDTDeviceNode>();
        if (ids == nullptr || fdt_node == nullptr) {
            return result;
        }

        for (int index = 0; ids[index].compatible != nullptr; ++index) {
            if ((ids[index].compatible[0] == '\0') ||
                fdt_node->is_compatible_with(ids[index].compatible) >= 0)
            {
                result.matched     = true;
                result.driver_flag = ids[index].driver_flag;
                result.index       = index;
                return result;
            }
        }
        return result;
    }

    [[nodiscard]]
    driver::MatchResult match_pci_ids(const driver::PCIDeviceId *ids,
                                      const device::DeviceNode &node) noexcept {
        driver::MatchResult result{};
        auto *pci_node = node.as<pci::PCIDeviceNode>();
        if (ids == nullptr || pci_node == nullptr) {
            return result;
        }

        const auto &identity = pci_node->identity();
        for (int index = 0; !is_end_id(ids[index]); ++index) {
            const auto &id = ids[index];
            const bool subvendor_match =
                id.subvendor_id == 0xFFFF ||
                identity.subvendor_id == id.subvendor_id;
            const bool subdevice_match =
                id.subdevice_id == 0xFFFF ||
                identity.subdevice_id == id.subdevice_id;
            const bool class_match =
                id.class_mask == 0 ||
                ((identity.class_code & id.class_mask) ==
                 (id.class_code & id.class_mask));
            if (identity.vendor_id != id.vendor_id ||
                identity.device_id != id.device_id || !subvendor_match ||
                !subdevice_match || !class_match)
            {
                continue;
            }

            result.matched     = true;
            result.driver_flag = id.driver_flag;
            result.index       = index;
            return result;
        }
        return result;
    }

    template <typename FactoryT>
    [[nodiscard]]
    driver::MatchResult match_factory_ids(const FactoryT &factory,
                                          const device::DeviceNode &node) noexcept {
        const auto &ids = factory.device_id();
        switch (node.platform()) {
            case device::DevicePlatform::FDT:
                return match_fdt_ids(ids.fdt_ids, node);
            case device::DevicePlatform::PCI:
                return match_pci_ids(ids.pci_ids, node);
            default:
                return {};
        }
    }
}  // namespace

namespace driver {
    bool IDeviceFactory::probe(const device::DeviceNode &node,
                               device::DeviceModel &model,
                               b64 driver_flag) const noexcept {
        (void)node;
        (void)model;
        (void)driver_flag;
        return true;
    }

    bool IIrqChipFactory::probe(const device::DeviceNode &node,
                                device::DeviceModel &model,
                                b64 driver_flag) const noexcept {
        (void)node;
        (void)model;
        (void)driver_flag;
        return true;
    }

    Result<void> DeviceFactoryRegistry::register_factory(
        const IDeviceFactory &factory) noexcept {
        for (auto *registered : _factories) {
            if (registered == &factory) {
                loggers::DEVICE::ERROR("DeviceFactory 重复注册: ptr=%p",
                                       &factory);
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
        }
        _factories.push_back(&factory);
        loggers::DEVICE::DEBUG("注册 DeviceFactory: ptr=%p", &factory);
        void_return();
    }

    MatchResult DeviceFactoryRegistry::match(const IDeviceFactory &factory,
                                             const device::DeviceNode &node) const
        noexcept {
        return match_factory_ids(factory, node);
    }

    Result<void> IrqChipFactoryRegistry::register_factory(
        const IIrqChipFactory &factory) noexcept {
        for (auto *registered : _factories) {
            if (registered == &factory) {
                loggers::DEVICE::ERROR("IrqChipFactory 重复注册: ptr=%p",
                                       &factory);
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
        }
        _factories.push_back(&factory);
        loggers::DEVICE::DEBUG("注册 IrqChipFactory: ptr=%p", &factory);
        void_return();
    }

    MatchResult IrqChipFactoryRegistry::match(
        const IIrqChipFactory &factory, const device::DeviceNode &node) const
        noexcept {
        return match_factory_ids(factory, node);
    }
}  // namespace driver
