/**
 * @file pci.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCI 设备模型与 provider
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/device.h>
#include <device/fdt/device_node.h>
#include <device/model.h>

#include <array>
#include <vector>

namespace pci {
    struct Bdf {
        b16 segment = 0;
        b8 bus = 0;
        b8 device = 0;
        b8 function = 0;
    };

    struct PCIDeviceIdentity {
        Bdf bdf{};
        b16 vendor_id = 0;
        b16 subvendor_id = 0;
        b16 device_id = 0;
        b16 subdevice_id = 0;
        b32 class_code = 0;
        b8 revision_id = 0;
        b8 header_type = 0;
    };

    struct PCIBar {
        bool valid = false;
        bool mmio = true;
        bool is_64bit = false;
        bool prefetchable = false;
        b8 index = 0;
        b64 pci_addr = 0;
        b64 cpu_addr = 0;
        b64 size = 0;
    };

    struct PCIHostRange {
        b32 flags = 0;
        b64 pci_addr = 0;
        b64 cpu_addr = 0;
        b64 size = 0;
        b64 alloc_next = 0;
    };

    struct PCIHostControllerConfig {
        b16 segment = 0;
        b8 root_bus = 0;
        b8 bus_start = 0;
        b8 bus_end = 0;
        PhyArea ecam_region{};
        KvaAddr ecam_base = KvaAddr::null;
        std::vector<PCIHostRange> ranges;
    };

    class PCIConfigOps {
    public:
        virtual ~PCIConfigOps() = default;

        [[nodiscard]]
        virtual b8 read8(const Bdf &bdf, b16 offset) const noexcept = 0;
        [[nodiscard]]
        virtual b16 read16(const Bdf &bdf, b16 offset) const noexcept = 0;
        [[nodiscard]]
        virtual b32 read32(const Bdf &bdf, b16 offset) const noexcept = 0;
        virtual void write32(const Bdf &bdf, b16 offset,
                             b32 value) const noexcept = 0;
    };

    class PCIDeviceNode final : public device::DeviceNode {
    public:
        static constexpr device::DevicePlatform IDENTIFIER =
            device::DevicePlatform::PCI;

        PCIDeviceNode(PCIDeviceIdentity identity, PCIHostControllerConfig host,
                      std::vector<PCIBar> bars,
                      std::vector<PhyArea> mmios,
                      std::vector<device::RawIrqSpec> irqs) noexcept;
        ~PCIDeviceNode() override = default;

        [[nodiscard]]
        const char *name() const noexcept override;

        [[nodiscard]]
        std::vector<PhyArea> mmio_regions() const noexcept override;

        [[nodiscard]]
        std::vector<device::RawIrqSpec> irq_specs() const noexcept override;

        [[nodiscard]]
        const PCIDeviceIdentity &identity() const noexcept {
            return _identity;
        }

        [[nodiscard]]
        const std::vector<PCIBar> &bars() const noexcept {
            return _bars;
        }

        [[nodiscard]]
        const PCIBar *find_bar(b8 index) const noexcept;

        [[nodiscard]]
        b8 read_config8(b16 offset) const noexcept;
        [[nodiscard]]
        b16 read_config16(b16 offset) const noexcept;
        [[nodiscard]]
        b32 read_config32(b16 offset) const noexcept;

        [[nodiscard]]
        b8 capability_pointer() const noexcept;
        [[nodiscard]]
        b16 find_capability(b8 cap_id) const noexcept;

    protected:
        [[nodiscard]]
        device::DevicePlatform type_id() const override {
            return IDENTIFIER;
        }

    private:
        class ConfigAccessor final : public PCIConfigOps {
        public:
            explicit ConfigAccessor(PCIHostControllerConfig config) noexcept
                : _config(std::move(config)) {}

            [[nodiscard]]
            b8 read8(const Bdf &bdf, b16 offset) const noexcept override;
            [[nodiscard]]
            b16 read16(const Bdf &bdf, b16 offset) const noexcept override;
            [[nodiscard]]
            b32 read32(const Bdf &bdf, b16 offset) const noexcept override;
            void write32(const Bdf &bdf, b16 offset,
                         b32 value) const noexcept override;

        private:
            [[nodiscard]]
            volatile void *config_ptr(const Bdf &bdf,
                                      b16 offset) const noexcept;

            PCIHostControllerConfig _config;
        };

        PCIDeviceIdentity _identity{};
        PCIHostControllerConfig _host{};
        std::vector<PCIBar> _bars;
        std::vector<PhyArea> _mmios;
        std::vector<device::RawIrqSpec> _irqs;
        std::array<char, 32> _name{};
    };

    class PCIBusProvider final : public device::DeviceProvider {
    public:
        explicit PCIBusProvider(const ::fdt::FDTDeviceNode &host) noexcept;
        ~PCIBusProvider() override = default;

        [[nodiscard]]
        Result<void> register_device(device::DeviceModel &model) const override;

        [[nodiscard]]
        const char *name() const override {
            return "pci-bus";
        }

    private:
        class EcamConfigOps final : public PCIConfigOps {
        public:
            explicit EcamConfigOps(PCIHostControllerConfig config) noexcept
                : _config(std::move(config)) {}

            [[nodiscard]]
            b8 read8(const Bdf &bdf, b16 offset) const noexcept override;
            [[nodiscard]]
            b16 read16(const Bdf &bdf, b16 offset) const noexcept override;
            [[nodiscard]]
            b32 read32(const Bdf &bdf, b16 offset) const noexcept override;
            void write32(const Bdf &bdf, b16 offset,
                         b32 value) const noexcept override;

        private:
            [[nodiscard]]
            volatile void *config_ptr(const Bdf &bdf,
                                      b16 offset) const noexcept;

            PCIHostControllerConfig _config;
        };

        [[nodiscard]]
        Result<PCIHostControllerConfig> parse_host_config() const noexcept;
        [[nodiscard]]
        Result<std::vector<PCIBar>> read_bars(
            const PCIHostControllerConfig &config,
            const PCIConfigOps &ops, const PCIDeviceIdentity &identity) const
            noexcept;
        [[nodiscard]]
        Result<void> enumerate_root_bus(
            device::DeviceModel &model,
            const PCIHostControllerConfig &config) const noexcept;
        [[nodiscard]]
        Result<void> enumerate_function(
            device::DeviceModel &model, const PCIHostControllerConfig &config,
            const PCIConfigOps &ops, const Bdf &bdf) const noexcept;
        [[nodiscard]]
        Result<void> log_and_register_device(
            device::DeviceModel &model, const PCIHostControllerConfig &config,
            PCIDeviceIdentity identity, std::vector<PCIBar> bars) const noexcept;

        const ::fdt::FDTDeviceNode *_host = nullptr;
    };
}  // namespace pci
