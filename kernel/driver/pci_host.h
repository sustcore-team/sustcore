/**
 * @file pci_host.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCI host ECAM 总线工厂
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/base.h>
#include <driver/factory.h>

namespace pci {
    class PCIHostDriver final : public driver::DriverBase {
    public:
        explicit PCIHostDriver(DevRes res) noexcept
            : DriverBase(std::move(res)) {}
        ~PCIHostDriver() override = default;

        [[nodiscard]]
        Result<void> mount(CapIdx devdir) noexcept override {
            (void)devdir;
            void_return();
        }
    };

    class PCIHostFactory final : public driver::IDeviceFactory {
    public:
        [[nodiscard]]
        const driver::DeviceId &device_id() const noexcept override;

        [[nodiscard]]
        bool probe(const device::DeviceNode &node, device::DeviceModel &model,
                   b64 driver_flag) const noexcept override;

        [[nodiscard]]
        Result<driver::DriverBase *> create(
            const device::DeviceNode &node, device::DeviceModel &model,
            b64 driver_flag) const override;
    };
}  // namespace pci
