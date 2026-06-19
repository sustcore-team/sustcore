/**
 * @file factory.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备工厂注册表
 * @version alpha-1.0.0
 * @date 2026-05-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <fwd.h>
#include <device/device.h>
#include <driver/base.h>
#include <sustcore/errcode.h>

#include <vector>

namespace driver {
    struct FDTDeviceId {
        const char *compatible = nullptr;
        b64 driver_flag        = 0;
    };

    struct PCIDeviceId {
        b16 vendor_id     = 0;
        b16 subvendor_id  = 0;
        b16 device_id     = 0;
        b16 subdevice_id  = 0;
        b32 class_code    = 0;
        b32 class_mask    = 0;
        b64 driver_flag   = 0;
    };

    struct DeviceId {
        const FDTDeviceId *fdt_ids = nullptr;
        const PCIDeviceId *pci_ids = nullptr;
    };

    struct MatchResult {
        bool matched     = false;
        b64 driver_flag  = 0;
        int index        = -1;
    };

    class IDeviceFactory {
    public:
        virtual ~IDeviceFactory() = default;

        [[nodiscard]]
        virtual const DeviceId &device_id() const noexcept = 0;

        [[nodiscard]]
        virtual bool probe(const device::DeviceNode &node,
                           device::DeviceModel &model,
                           b64 driver_flag) const noexcept;

        [[nodiscard]]
        virtual Result<DriverBase *> create(
            const device::DeviceNode &node, device::DeviceModel &model,
            b64 driver_flag) const = 0;
    };

    class IIrqChipFactory {
    public:
        virtual ~IIrqChipFactory() = default;

        [[nodiscard]]
        virtual const DeviceId &device_id() const noexcept = 0;

        [[nodiscard]]
        virtual bool probe(const device::DeviceNode &node,
                           device::DeviceModel &model,
                           b64 driver_flag) const noexcept;

        [[nodiscard]]
        virtual Result<DriverBase *> create(
            const device::DeviceNode &node, device::DeviceModel &model,
            b64 driver_flag) const = 0;
    };

    class DeviceFactoryRegistry {
    public:
        [[nodiscard]]
        Result<void> register_factory(const IDeviceFactory &factory) noexcept;
        [[nodiscard]]
        const std::vector<const IDeviceFactory *> &factories() const noexcept {
            return _factories;
        }
        [[nodiscard]]
        MatchResult match(const IDeviceFactory &factory,
                          const device::DeviceNode &node) const noexcept;

    private:
        std::vector<const IDeviceFactory *> _factories;
    };

    class IrqChipFactoryRegistry {
    public:
        [[nodiscard]]
        Result<void> register_factory(const IIrqChipFactory &factory) noexcept;
        [[nodiscard]]
        const std::vector<const IIrqChipFactory *> &factories() const noexcept {
            return _factories;
        }
        [[nodiscard]]
        MatchResult match(const IIrqChipFactory &factory,
                          const device::DeviceNode &node) const noexcept;

    private:
        std::vector<const IIrqChipFactory *> _factories;
    };
}  // namespace driver
