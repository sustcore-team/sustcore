/**
 * @file pci.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCI 设备模型与 provider 实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/fdt/decode.h>
#include <device/pci.h>
#include <device/resource.h>
#include <guard.h>
#include <logger.h>
#include <sustcore/addr.h>

#include <cstdio>

namespace {
    constexpr const char *PCI_HOST_ECAM_GENERIC = "pci-host-ecam-generic";
    constexpr const char *BUS_RANGE_PROP        = "bus-range";
    constexpr const char *RANGES_PROP           = "ranges";
    constexpr const char *PCI_DOMAIN_PROP       = "linux,pci-domain";

    constexpr b16 PCI_VENDOR_ID       = 0x00;
    constexpr b16 PCI_DEVICE_ID       = 0x02;
    constexpr b16 PCI_REVISION_ID     = 0x08;
    constexpr b16 PCI_PROG_IF         = 0x09;
    constexpr b16 PCI_SUBCLASS        = 0x0A;
    constexpr b16 PCI_CLASS_CODE      = 0x0B;
    constexpr b16 PCI_HEADER_TYPE     = 0x0E;
    constexpr b16 PCI_BAR0            = 0x10;
    constexpr b16 PCI_SUBVENDOR_ID    = 0x2C;
    constexpr b16 PCI_SUBDEVICE_ID    = 0x2E;
    constexpr b16 PCI_SECONDARY_BUS   = 0x19;
    constexpr b16 PCI_COMMAND         = 0x04;
    constexpr b8 PCI_HEADER_MULTIFUNC = 0x80;
    constexpr b8 PCI_HEADER_TYPE_MASK = 0x7F;
    constexpr b8 PCI_HEADER_BRIDGE    = 0x01;
    constexpr b32 PCI_BAR_IO_MASK     = 0x1;
    constexpr b32 PCI_BAR_TYPE_MASK   = 0x6;
    constexpr b32 PCI_BAR_PREFETCH    = 0x8;
    constexpr b32 PCI_BAR_MEM_MASK    = 0xFFFFFFF0u;
    constexpr b64 PCI_BAR_MEM64_MASK  = 0xFFFFFFFFFFFFFFF0ULL;

    [[nodiscard]]
    b64 read_be_cell_value(const char *data, size_t cells) noexcept {
        b64 value = 0;
        if (data == nullptr) {
            return 0;
        }
        for (size_t i = 0; i < cells; ++i) {
            uint64_t cell = 0;
            if (!fdt::detail::parse_be_integer(data + i * sizeof(sus_u32),
                                               sizeof(sus_u32), cell))
            {
                return 0;
            }
            value = (value << 32) | cell;
        }
        return value;
    }

    [[nodiscard]]
    bool pci_range_is_mmio(b32 flags) noexcept {
        const auto space_code = static_cast<b8>((flags >> 24) & 0x3);
        return space_code == 0x2 || space_code == 0x3;
    }

    [[nodiscard]]
    std::vector<pci::PCIHostRange> parse_pci_ranges(
        const fdt::Property &prop, const fdt::Node &node) noexcept {
        std::vector<pci::PCIHostRange> ranges;
        auto *parent = node.parent;
        if (prop.data == nullptr || parent == nullptr) {
            return ranges;
        }

        auto parent_cells = fdt::node_region_cells(*parent);
        const size_t child_addr_cells = 3;
        const size_t parent_addr_cells = parent_cells.addr_cells;
        const size_t size_cells = 2;
        const size_t cells_per_range =
            child_addr_cells + parent_addr_cells + size_cells;
        if (cells_per_range == 0 ||
            prop.size % (cells_per_range * sizeof(sus_u32)) != 0)
        {
            return ranges;
        }

        const auto total = prop.size / (cells_per_range * sizeof(sus_u32));
        ranges.reserve(total);
        const char *cursor = prop.data;
        for (size_t i = 0; i < total; ++i) {
            auto flags = static_cast<b32>(
                read_be_cell_value(cursor, 1));
            cursor += sizeof(sus_u32);
            auto pci_hi = read_be_cell_value(cursor, 2);
            cursor += 2 * sizeof(sus_u32);
            auto cpu_addr = read_be_cell_value(cursor, parent_addr_cells);
            cursor += parent_addr_cells * sizeof(sus_u32);
            auto size = read_be_cell_value(cursor, size_cells);
            cursor += size_cells * sizeof(sus_u32);
            ranges.push_back(pci::PCIHostRange{
                .flags = flags,
                .pci_addr = pci_hi,
                .cpu_addr = cpu_addr,
                .size = size,
            });
        }
        return ranges;
    }

    [[nodiscard]]
    b64 translate_pci_addr(const std::vector<pci::PCIHostRange> &ranges,
                           b64 pci_addr) noexcept {
        for (const auto &range : ranges) {
            if (!pci_range_is_mmio(range.flags)) {
                continue;
            }
            if (pci_addr >= range.pci_addr &&
                pci_addr < range.pci_addr + range.size)
            {
                return range.cpu_addr + (pci_addr - range.pci_addr);
            }
        }
        return 0;
    }

    [[nodiscard]]
    std::optional<b64> allocate_pci_bar_address(
        std::vector<pci::PCIHostRange> &ranges, b64 size, bool is_64bit) noexcept {
        if (size == 0) {
            return std::nullopt;
        }

        for (auto &range : ranges) {
            if (!pci_range_is_mmio(range.flags)) {
                continue;
            }
            if (!is_64bit &&
                range.pci_addr + range.size > static_cast<b64>(UINT32_MAX))
            {
                continue;
            }

            b64 alloc = range.alloc_next == 0 ? range.pci_addr : range.alloc_next;
            alloc = page_align_up(alloc);
            if (alloc < range.pci_addr ||
                alloc + size > range.pci_addr + range.size)
            {
                continue;
            }
            if (!is_64bit &&
                alloc + size > static_cast<b64>(UINT32_MAX))
            {
                continue;
            }

            range.alloc_next = alloc + size;
            return alloc;
        }
        return std::nullopt;
    }
}  // namespace

namespace pci {
    PCIDeviceNode::PCIDeviceNode(PCIDeviceIdentity identity,
                                 PCIHostControllerConfig host,
                                 std::vector<PCIBar> bars,
                                 std::vector<PhyArea> mmios,
                                 std::vector<device::RawIrqSpec> irqs) noexcept
        : _identity(identity),
          _host(std::move(host)),
          _bars(std::move(bars)),
          _mmios(std::move(mmios)),
          _irqs(std::move(irqs)) {
        snprintf(_name.data(), _name.size(), "pci-%04x:%02x:%02x.%u",
                 _identity.bdf.segment, _identity.bdf.bus,
                 _identity.bdf.device, _identity.bdf.function);
    }

    const char *PCIDeviceNode::name() const noexcept {
        return _name.data();
    }

    std::vector<PhyArea> PCIDeviceNode::mmio_regions() const noexcept {
        return _mmios;
    }

    std::vector<device::RawIrqSpec> PCIDeviceNode::irq_specs() const noexcept {
        return _irqs;
    }

    const PCIBar *PCIDeviceNode::find_bar(b8 index) const noexcept {
        for (const auto &bar : _bars) {
            if (bar.index == index) {
                return &bar;
            }
        }
        return nullptr;
    }

    volatile void *PCIDeviceNode::ConfigAccessor::config_ptr(
        const Bdf &bdf, b16 offset) const noexcept {
        if (offset >= 0x1000 || bdf.device > 31 || bdf.function > 7 ||
            bdf.bus < _config.bus_start || bdf.bus > _config.bus_end)
        {
            return nullptr;
        }
        auto ecam_kva = convert<KvaAddr>(_config.ecam_region.begin);
        const auto offset_bytes =
            (static_cast<b64>(bdf.bus - _config.bus_start) << 20) |
            (static_cast<b64>(bdf.device) << 15) |
            (static_cast<b64>(bdf.function) << 12) | offset;
        return static_cast<volatile char *>(ecam_kva.addr()) + offset_bytes;
    }

    b8 PCIDeviceNode::ConfigAccessor::read8(const Bdf &bdf,
                                            b16 offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile b8 *>(config_ptr(bdf, offset));
        return ptr != nullptr ? *ptr : 0xFF;
    }

    b16 PCIDeviceNode::ConfigAccessor::read16(const Bdf &bdf,
                                              b16 offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile b16 *>(config_ptr(bdf, offset));
        return ptr != nullptr ? *ptr : 0xFFFF;
    }

    b32 PCIDeviceNode::ConfigAccessor::read32(const Bdf &bdf,
                                              b16 offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile b32 *>(config_ptr(bdf, offset));
        return ptr != nullptr ? *ptr : 0xFFFFFFFFu;
    }

    void PCIDeviceNode::ConfigAccessor::write32(const Bdf &bdf, b16 offset,
                                                b32 value) const noexcept {
        auto *ptr = reinterpret_cast<volatile b32 *>(config_ptr(bdf, offset));
        if (ptr != nullptr) {
            *ptr = value;
        }
    }

    b8 PCIDeviceNode::read_config8(b16 offset) const noexcept {
        return ConfigAccessor(_host).read8(_identity.bdf, offset);
    }

    b16 PCIDeviceNode::read_config16(b16 offset) const noexcept {
        return ConfigAccessor(_host).read16(_identity.bdf, offset);
    }

    b32 PCIDeviceNode::read_config32(b16 offset) const noexcept {
        return ConfigAccessor(_host).read32(_identity.bdf, offset);
    }

    b8 PCIDeviceNode::capability_pointer() const noexcept {
        return read_config8(0x34);
    }

    b16 PCIDeviceNode::find_capability(b8 cap_id) const noexcept {
        b16 current = capability_pointer();
        size_t limit = 64;
        while (current != 0 && current < 0x1000 && limit-- > 0) {
            if (read_config8(current) == cap_id) {
                return current;
            }
            current = read_config8(static_cast<b16>(current + 1));
        }
        return 0;
    }

    PCIBusProvider::PCIBusProvider(const fdt::FDTDeviceNode &host) noexcept
        : _host(&host) {}

    volatile void *PCIBusProvider::EcamConfigOps::config_ptr(
        const Bdf &bdf, b16 offset) const noexcept {
        if (offset >= 0x1000 || bdf.device > 31 || bdf.function > 7 ||
            bdf.bus < _config.bus_start || bdf.bus > _config.bus_end ||
            !_config.ecam_base.nonnull())
        {
            return nullptr;
        }
        const auto offset_bytes =
            (static_cast<b64>(bdf.bus - _config.bus_start) << 20) |
            (static_cast<b64>(bdf.device) << 15) |
            (static_cast<b64>(bdf.function) << 12) | offset;
        return static_cast<volatile char *>(_config.ecam_base.addr()) +
               offset_bytes;
    }

    b8 PCIBusProvider::EcamConfigOps::read8(const Bdf &bdf,
                                            b16 offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile b8 *>(config_ptr(bdf, offset));
        return ptr != nullptr ? *ptr : 0xFF;
    }

    b16 PCIBusProvider::EcamConfigOps::read16(const Bdf &bdf,
                                              b16 offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile b16 *>(config_ptr(bdf, offset));
        return ptr != nullptr ? *ptr : 0xFFFF;
    }

    b32 PCIBusProvider::EcamConfigOps::read32(const Bdf &bdf,
                                              b16 offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile b32 *>(config_ptr(bdf, offset));
        return ptr != nullptr ? *ptr : 0xFFFFFFFFu;
    }

    void PCIBusProvider::EcamConfigOps::write32(const Bdf &bdf, b16 offset,
                                                b32 value) const noexcept {
        auto *ptr = reinterpret_cast<volatile b32 *>(config_ptr(bdf, offset));
        if (ptr != nullptr) {
            *ptr = value;
        }
    }

    Result<PCIHostControllerConfig> PCIBusProvider::parse_host_config() const
        noexcept {
        if (_host == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (_host->is_compatible_with(PCI_HOST_ECAM_GENERIC) < 0) {
            loggers::DEVICE::ERROR("PCI host %s compatible 不受支持",
                                   _host->name());
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        const auto &node = _host->raw_node();
        auto reg_it = node.properties.find(fdt::REG_PROP);
        auto bus_range_it = node.properties.find(BUS_RANGE_PROP);
        auto ranges_it = node.properties.find(RANGES_PROP);
        if (reg_it == node.properties.end() || bus_range_it == node.properties.end() ||
            ranges_it == node.properties.end())
        {
            loggers::DEVICE::ERROR("PCI host %s 缺少 reg/bus-range/ranges",
                                   _host->name());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto ecam_regions = reg_it->second->as_regions(fdt::node_region_cells(node));
        if (ecam_regions.empty()) {
            loggers::DEVICE::ERROR("PCI host %s 的 ECAM reg 非法", _host->name());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto bus_range_values =
            fdt::parse_u32_cells(*bus_range_it->second.get());
        if (bus_range_values.size() < 2) {
            loggers::DEVICE::ERROR("PCI host %s 的 bus-range 非法",
                                   _host->name());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        b16 segment = 0;
        auto domain_it = node.properties.find(PCI_DOMAIN_PROP);
        if (domain_it != node.properties.end()) {
            segment = static_cast<b16>(domain_it->second->as_integral());
        }

        auto ranges = parse_pci_ranges(*ranges_it->second.get(), node);
        if (ranges.empty()) {
            loggers::DEVICE::WARN("PCI host %s 未解析到可用 MMIO ranges",
                                  _host->name());
        }

        auto config = PCIHostControllerConfig{
            .segment = segment,
            .root_bus = static_cast<b8>(bus_range_values[0]),
            .bus_start = static_cast<b8>(bus_range_values[0]),
            .bus_end = static_cast<b8>(bus_range_values[1]),
            .ecam_region = ecam_regions.front(),
            .ranges = std::move(ranges),
        };
        return config;
    }

    Result<std::vector<PCIBar>> PCIBusProvider::read_bars(
        const PCIHostControllerConfig &config, const PCIConfigOps &ops,
        const PCIDeviceIdentity &identity) const noexcept {
        std::vector<PCIBar> bars;
        const auto header_type =
            static_cast<b8>(identity.header_type & PCI_HEADER_TYPE_MASK);
        const size_t bar_limit = header_type == PCI_HEADER_BRIDGE ? 2 : 6;
        bars.reserve(bar_limit);

        for (size_t index = 0; index < bar_limit; ++index) {
            const auto bar_offset = static_cast<b16>(PCI_BAR0 + index * 4);
            auto bar_low = ops.read32(identity.bdf, bar_offset);
            if (bar_low == 0 || bar_low == 0xFFFFFFFFu) {
                continue;
            }

            if ((bar_low & PCI_BAR_IO_MASK) != 0) {
                bars.push_back(PCIBar{
                    .valid = true,
                    .mmio = false,
                    .index = static_cast<b8>(index),
                });
                continue;
            }

            const auto type =
                static_cast<b8>((bar_low & PCI_BAR_TYPE_MASK) >> 1);
            const bool is_64bit = type == 0x2;
            b64 raw_addr = static_cast<b64>(bar_low & PCI_BAR_MEM_MASK);
            b32 size_low_mask = 0;
            b32 orig_high = 0;
            b32 size_high_mask = 0;
            const auto current_index = index;
            if (is_64bit && index + 1 < bar_limit) {
                auto bar_high = ops.read32(identity.bdf, bar_offset + 4);
                raw_addr |= static_cast<b64>(bar_high) << 32;
                orig_high = bar_high;
                ++index;
            }

            ops.write32(identity.bdf, bar_offset, 0xFFFFFFFFu);
            size_low_mask = ops.read32(identity.bdf, bar_offset);
            ops.write32(identity.bdf, bar_offset, bar_low);
            if (is_64bit) {
                ops.write32(identity.bdf, bar_offset + 4, 0xFFFFFFFFu);
                size_high_mask = ops.read32(identity.bdf, bar_offset + 4);
                ops.write32(identity.bdf, bar_offset + 4, orig_high);
            }

            b64 size = 0;
            if (is_64bit) {
                const b64 size_mask = (static_cast<b64>(size_high_mask) << 32) |
                                      static_cast<b64>(size_low_mask &
                                                       PCI_BAR_MEM_MASK);
                if (size_mask != 0 && size_mask != PCI_BAR_MEM64_MASK) {
                    size = (~size_mask) + 1;
                }
            } else {
                const b32 size_mask = size_low_mask & PCI_BAR_MEM_MASK;
                if (size_mask != 0 && size_mask != PCI_BAR_MEM_MASK) {
                    size = static_cast<b64>((~size_mask) + 1);
                }
            }

            if (raw_addr == 0) {
                auto allocated =
                    allocate_pci_bar_address(const_cast<PCIHostControllerConfig &>(
                                                 config)
                                                 .ranges,
                                             size, is_64bit);
                if (allocated.has_value()) {
                    raw_addr = *allocated;
                    auto new_low = static_cast<b32>((raw_addr & 0xFFFFFFFFu) |
                                                    (bar_low & 0xFu));
                    ops.write32(identity.bdf, bar_offset, new_low);
                    if (is_64bit && index > current_index) {
                        auto new_high =
                            static_cast<b32>((raw_addr >> 32) & 0xFFFFFFFFu);
                        ops.write32(identity.bdf,
                                    static_cast<b16>(bar_offset + 4), new_high);
                    }
                    loggers::DEVICE::INFO(
                        "为 PCI BAR%u 分配地址: %04x:%02x:%02x.%u pci=0x%llx size=0x%llx",
                        static_cast<unsigned>(current_index),
                        identity.bdf.segment, identity.bdf.bus,
                        identity.bdf.device, identity.bdf.function,
                        static_cast<unsigned long long>(raw_addr),
                        static_cast<unsigned long long>(size));
                }
            }

            b64 cpu_addr = translate_pci_addr(config.ranges, raw_addr);
            if (cpu_addr == 0) {
                loggers::DEVICE::WARN(
                    "PCI 设备 %04x:%02x:%02x.%u BAR 未命中 host ranges: pci=0x%llx",
                    identity.bdf.segment, identity.bdf.bus, identity.bdf.device,
                    identity.bdf.function,
                    static_cast<unsigned long long>(raw_addr));
                continue;
            }

            bars.push_back(PCIBar{
                .valid = true,
                .mmio = true,
                .is_64bit = is_64bit,
                .prefetchable = (bar_low & PCI_BAR_PREFETCH) != 0,
                .index = static_cast<b8>(current_index),
                .pci_addr = raw_addr,
                .cpu_addr = cpu_addr,
                .size = size,
            });
        }
        return bars;
    }

    Result<void> PCIBusProvider::log_and_register_device(
        device::DeviceModel &model, const PCIHostControllerConfig &config,
        PCIDeviceIdentity identity,
        std::vector<PCIBar> bars) const noexcept {
        std::vector<PhyArea> mmios;
        mmios.reserve(bars.size());

        loggers::DEVICE::INFO(
            "发现 PCI 设备: %04x:%02x:%02x.%u vendor=%04x device=%04x class=%06x header=%02x rev=%02x",
            identity.bdf.segment, identity.bdf.bus, identity.bdf.device,
            identity.bdf.function, identity.vendor_id, identity.device_id,
            identity.class_code, identity.header_type, identity.revision_id);

        for (const auto &bar : bars) {
            if (!bar.valid || !bar.mmio || bar.cpu_addr == 0) {
                continue;
            }
            loggers::DEVICE::INFO(
                "PCI BAR%u: pci=0x%llx cpu=0x%llx size=0x%llx prefetch=%s",
                static_cast<unsigned>(bar.index),
                static_cast<unsigned long long>(bar.pci_addr),
                static_cast<unsigned long long>(bar.cpu_addr),
                static_cast<unsigned long long>(bar.size),
                bar.prefetchable ? "yes" : "no");
            if (bar.size != 0) {
                mmios.push_back(PhyArea(
                    PhyAddr(static_cast<addr_t>(bar.cpu_addr)),
                    PhyAddr(static_cast<addr_t>(bar.cpu_addr + bar.size))));
            }
        }

        auto node_res = model.register_device_node(
            util::owner<device::DeviceNode *>(
                new PCIDeviceNode(identity, config, std::move(bars),
                                  std::move(mmios), {})));
        propagate(node_res);
        void_return();
    }

    Result<void> PCIBusProvider::enumerate_function(
        device::DeviceModel &model, const PCIHostControllerConfig &config,
        const PCIConfigOps &ops, const Bdf &bdf) const noexcept {
        auto vendor_id = ops.read16(bdf, PCI_VENDOR_ID);
        if (vendor_id == 0xFFFF) {
            void_return();
        }

        auto header_type = ops.read8(bdf, PCI_HEADER_TYPE);
        auto class_code =
            (static_cast<b32>(ops.read8(bdf, PCI_CLASS_CODE)) << 16) |
            (static_cast<b32>(ops.read8(bdf, PCI_SUBCLASS)) << 8) |
            static_cast<b32>(ops.read8(bdf, PCI_PROG_IF));

        auto identity = PCIDeviceIdentity{
            .bdf = bdf,
            .vendor_id = vendor_id,
            .subvendor_id = ops.read16(bdf, PCI_SUBVENDOR_ID),
            .device_id = ops.read16(bdf, PCI_DEVICE_ID),
            .subdevice_id = ops.read16(bdf, PCI_SUBDEVICE_ID),
            .class_code = class_code,
            .revision_id = ops.read8(bdf, PCI_REVISION_ID),
            .header_type = header_type,
        };

        auto bars_res = read_bars(config, ops, identity);
        propagate(bars_res);

        if ((header_type & PCI_HEADER_TYPE_MASK) == PCI_HEADER_BRIDGE) {
            auto secondary = ops.read8(bdf, PCI_SECONDARY_BUS);
            loggers::DEVICE::INFO(
                "发现 PCI bridge: %04x:%02x:%02x.%u secondary=%u (第一阶段不递归)",
                bdf.segment, bdf.bus, bdf.device, bdf.function,
                static_cast<unsigned>(secondary));
        }

        auto command = ops.read16(bdf, PCI_COMMAND);
        command |= (1u << 0);
        command |= (1u << 1);
        command |= (1u << 2);
        auto command_low = static_cast<b32>(command);
        auto command_full = ops.read32(bdf, PCI_COMMAND & ~0x3u);
        command_full = (command_full & 0xFFFF0000u) | command_low;
        ops.write32(bdf, PCI_COMMAND & ~0x3u, command_full);

        auto register_res =
            log_and_register_device(model, config, identity,
                                    std::move(bars_res.value()));
        propagate(register_res);
        void_return();
    }

    Result<void> PCIBusProvider::enumerate_root_bus(
        device::DeviceModel &model,
        const PCIHostControllerConfig &config) const noexcept {
        auto ecam_resource = device::MMIOResource::make(config.ecam_region);
        if (ecam_resource.get() == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto map_res = device::MMIOManager::inst().map_to_kernel(
            *ecam_resource.get());
        propagate(map_res);
        auto unmap_guard = util::Guard([resource = ecam_resource.get()]() {
            auto unmap_res = device::MMIOManager::inst().unmap_from_kernel(
                *resource);
            if (!unmap_res.has_value()) {
                loggers::DEVICE::ERROR("解除 PCI ECAM 映射失败: err=%s",
                                       to_cstring(unmap_res.error()));
            }
        });

        auto runtime_config = config;
        runtime_config.ecam_base = map_res.value();
        EcamConfigOps ops(runtime_config);
        for (b8 device = 0; device < 32; ++device) {
            Bdf bdf{
                .segment = config.segment,
                .bus = config.root_bus,
                .device = device,
                .function = 0,
            };
            auto vendor_id = ops.read16(bdf, PCI_VENDOR_ID);
            if (vendor_id == 0xFFFF) {
                continue;
            }

            auto function0_res = enumerate_function(model, config, ops, bdf);
            propagate(function0_res);

            auto header_type = ops.read8(bdf, PCI_HEADER_TYPE);
            if ((header_type & PCI_HEADER_MULTIFUNC) == 0) {
                continue;
            }

            for (b8 function = 1; function < 8; ++function) {
                bdf.function = function;
                auto func_res = enumerate_function(model, config, ops, bdf);
                propagate(func_res);
            }
        }
        void_return();
    }

    Result<void> PCIBusProvider::register_device(
        device::DeviceModel &model) const {
        auto config_res = parse_host_config();
        propagate(config_res);
        const auto &config = config_res.value();

        loggers::DEVICE::INFO(
            "注册 PCI host provider: node=%s domain=%u ecam=[%p,%p) bus-range=[%u,%u]",
            _host != nullptr ? _host->name() : "unknown",
            static_cast<unsigned>(config.segment),
            config.ecam_region.begin.addr(), config.ecam_region.end.addr(),
            static_cast<unsigned>(config.bus_start),
            static_cast<unsigned>(config.bus_end));
        loggers::DEVICE::INFO("PCI host ranges 数量=%u",
                              static_cast<unsigned>(config.ranges.size()));

        auto enum_res = enumerate_root_bus(model, config);
        propagate(enum_res);
        void_return();
    }
}  // namespace pci
