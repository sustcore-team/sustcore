/**
 * @file pci_host.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCI host ECAM 总线工厂实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/pci.h>
#include <driver/pci_host.h>
#include <logger.h>

namespace pci {
    namespace {
        constexpr driver::FDTDeviceId PCI_HOST_FDT_IDS[] = {
            {.compatible = "pci-host-ecam-generic", .driver_flag = 0},
            {.compatible = nullptr, .driver_flag = 0},
        };
        constexpr driver::DeviceId PCI_HOST_DEVICE_ID = {
            .fdt_ids = PCI_HOST_FDT_IDS,
            .pci_ids = nullptr,
        };
    }  // namespace

    const driver::DeviceId &PCIHostFactory::device_id() const noexcept {
        return PCI_HOST_DEVICE_ID;
    }

    bool PCIHostFactory::probe(const device::DeviceNode &node,
                               device::DeviceModel &model,
                               b64 driver_flag) const noexcept {
        (void)model;
        (void)driver_flag;
        auto *fdt_node = node.as<fdt::FDTDeviceNode>();
        return fdt_node != nullptr &&
               fdt_node->is_compatible_with("pci-host-ecam-generic") >= 0;
    }

    Result<driver::DriverBase *> PCIHostFactory::create(
        const device::DeviceNode &node, device::DeviceModel &model,
        b64 driver_flag) const {
        (void)driver_flag;
        auto *fdt_node = node.as<fdt::FDTDeviceNode>();
        if (fdt_node == nullptr) {
            loggers::DEVICE::ERROR("PCI host 工厂收到非 FDT 节点: %s",
                                   node.name());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto provider_res = model.register_provider(
            util::owner<device::DeviceProvider *>(new PCIBusProvider(*fdt_node)));
        propagate(provider_res);

        loggers::DEVICE::INFO("PCI host 已接入总线 provider: %s", node.name());
        auto *driver = new PCIHostDriver(
            driver::DriverBase::DevRes(node, {}, {}));
        if (driver == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        return driver;
    }
}  // namespace pci
