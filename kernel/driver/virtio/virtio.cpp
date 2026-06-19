/**
 * @file virtio.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtio MMIO 通用探测、初始化与队列管理
 * @version alpha-1.0.0
 * @date 2026-06-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/resource.h>
#include <driver/virtio/virtio.h>
#include <logger.h>
#include <mem/gfp.h>
#include <device/pci.h>
#include <sus/raii.h>
#include <task/scheduler.h>

#include <algorithm>
#include <cstring>
#include <ranges>

namespace virtio {
    namespace {
        constexpr driver::FDTDeviceId VIRTIO_MMIO_FDT_IDS[] = {
            {.compatible = "virtio,mmio", .driver_flag = 0},
            {.compatible = nullptr, .driver_flag = 0},
        };
        constexpr driver::PCIDeviceId VIRTIO_MMIO_PCI_IDS[] = {
            {
                .vendor_id = 0x1AF4,
                .subvendor_id = 0xFFFF,
                .device_id = 0x1001,
                .subdevice_id = 0xFFFF,
                .class_code = 0,
                .class_mask = 0,
                .driver_flag = 0,
            },
            {},
        };
        constexpr driver::DeviceId VIRTIO_MMIO_DEVICE_ID = {
            .fdt_ids = VIRTIO_MMIO_FDT_IDS,
            .pci_ids = VIRTIO_MMIO_PCI_IDS,
        };

        [[nodiscard]]
        const char *platform_name(device::DevicePlatform platform) noexcept {
            switch (platform) {
                case device::DevicePlatform::FDT: return "fdt";
                case device::DevicePlatform::PCI: return "pci";
                default:                          return "unknown";
            }
        }

        /**
         * @brief 计算 legacy 描述符区所需字节数.
         */
        constexpr size_t LEGACY_QUEUE_ALIGNMENT  = 4096;
        constexpr size_t MAX_QUEUE_SIZE          = 128;
        constexpr size_t POLL_RETRY_BEFORE_YIELD = 1024;

        std::vector<const IVirtioDeviceFactory *> g_virtio_factories;

        /**
         * @brief 判断探测结果是否足以视为当前实现支持的 virtio 设备.
         */
        [[nodiscard]]
        bool is_valid_device(const ProbeInfo &info) noexcept {
            return virtio::is_virtio_magic(info.magic_value) &&
                   virtio::is_supported_version(info.version) &&
                   info.device_id !=
                       static_cast<virtio::u32>(virtio::DeviceType::INVALID);
        }

        [[nodiscard]]
        std::optional<size_t> pci_common_reg_offset(size_t reg_offset) noexcept {
            switch (reg_offset) {
                case offset::DEVICE_FEATURE_SEL:
                    return offsetof(VirtioPciCommonCfg, device_feature_select);
                case offset::DEVICE_FEATURE:
                    return offsetof(VirtioPciCommonCfg, device_feature);
                case offset::DRIVER_FEATURE_SEL:
                    return offsetof(VirtioPciCommonCfg, driver_feature_select);
                case offset::DRIVER_FEATURE:
                    return offsetof(VirtioPciCommonCfg, driver_feature);
                case offset::QUEUE_SEL:
                    return offsetof(VirtioPciCommonCfg, queue_select);
                case offset::QUEUE_NUM_MAX:
                    return offsetof(VirtioPciCommonCfg, queue_size);
                case offset::QUEUE_NUM:
                    return offsetof(VirtioPciCommonCfg, queue_size);
                case offset::STATUS:
                    return offsetof(VirtioPciCommonCfg, device_status);
                default:
                    return std::nullopt;
            }
        }

        /**
         * @brief 计算 legacy 描述符区所需字节数.
         */
        [[nodiscard]]
        size_t queue_desc_bytes(virtio::u16 queue_size) noexcept {
            return static_cast<size_t>(queue_size) *
                   sizeof(VirtQueueDescLegacy);
        }

        /**
         * @brief 计算 legacy avail ring 所需字节数.
         */
        [[nodiscard]]
        size_t queue_avail_bytes(virtio::u16 queue_size) noexcept {
            return sizeof(virtio::le16) * (3 + static_cast<size_t>(queue_size));
        }

        /**
         * @brief 计算 legacy used ring 所需字节数.
         */
        [[nodiscard]]
        size_t queue_used_bytes(virtio::u16 queue_size) noexcept {
            return sizeof(virtio::le16) * 3 +
                   sizeof(VirtQueueUsedElemLegacy) *
                       static_cast<size_t>(queue_size);
        }

        /**
         * @brief 计算一条 legacy virtqueue 所需的总 DMA 字节数.
         *
         * 同时返回 avail 与 used 相对基址偏移, 便于后续构造 ring 视图.
         */
        [[nodiscard]]
        size_t queue_total_bytes(virtio::u16 queue_size, size_t *avail_offset,
                                 size_t *used_offset) noexcept {
            const size_t desc_bytes  = queue_desc_bytes(queue_size);
            const size_t avail_bytes = queue_avail_bytes(queue_size);
            const size_t used_bytes  = queue_used_bytes(queue_size);
            if (avail_offset != nullptr) {
                *avail_offset = desc_bytes;
            }
            const size_t aligned_used_offset =
                page_align_up(desc_bytes + avail_bytes);
            if (used_offset != nullptr) {
                *used_offset = aligned_used_offset;
            }
            return aligned_used_offset + page_align_up(used_bytes);
        }

        /**
         * @brief 在 virtio 子工厂注册表中按设备类型查找第一个接受的工厂.
         */
        [[nodiscard]]
        const IVirtioDeviceFactory *find_virtio_factory(
            const device::DeviceNode &node, device::DeviceModel &model,
            const ProbeInfo &info) noexcept {
            for (const auto *factory : g_virtio_factories) {
                if (factory == nullptr ||
                    static_cast<virtio::u32>(factory->device_type()) !=
                        info.device_id)
                {
                    continue;
                }
                if (!factory->probe(node, model, info)) {
                    loggers::DEVICE::DEBUG(
                        "virtio 子工厂拒绝接管设备: node=%s type=%s(%u)",
                        node.name(), virtio::device_type_name(info.device_id),
                        static_cast<unsigned>(info.device_id));
                    continue;
                }
                loggers::DEVICE::DEBUG(
                    "virtio 子工厂接受接管设备: node=%s type=%s(%u)",
                    node.name(), virtio::device_type_name(info.device_id),
                    static_cast<unsigned>(info.device_id));
                return factory;
            }
            return nullptr;
        }

        /**
         * @brief 输出一次通用探测日志.
         *
         * 该日志重点保留 transport、设备类型与公共寄存器关键信息,
         * 便于后续定位驱动匹配与初始化问题.
         */
        void log_probe_result(const device::DeviceNode &node,
                              const ProbeInfo &info) noexcept {
            const auto *mmio = info.mmio;
            auto mmio_begin =
                mmio != nullptr ? mmio->region().begin.addr() : nullptr;
            auto mmio_end =
                mmio != nullptr ? mmio->region().end.addr() : nullptr;

            loggers::DEVICE::DEBUG(
                "virtio 探测结果: node=%s platform=%s mmio=[%p,%p) valid=%s "
                "transport=%s magic=0x%08x version=%u device_id=%u type=%s "
                "vendor=0x%08x status=0x%08x(%s) features0=0x%08x",
                node.name(), platform_name(node.platform()), mmio_begin,
                mmio_end,
                info.valid ? "true" : "false",
                virtio::transport_version_name(info.version), info.magic_value,
                static_cast<unsigned>(info.version),
                static_cast<unsigned>(info.device_id),
                virtio::device_type_name(info.device_id),
                static_cast<unsigned>(info.vendor_id),
                static_cast<unsigned>(info.status),
                virtio::device_status_name(info.status),
                static_cast<unsigned>(info.device_features));
        }

        /**
         * @brief 对给定节点执行一次只读 MMIO 通用探测.
         *
         * 先校验资源与映射窗口, 然后读取公共寄存器, 最后在必要时输出探测日志.
         */
        [[nodiscard]]
        Result<ProbeInfo> probe_mmio_device_impl(
            const device::DeviceNode &node, bool emit_log,
            bool accept_invalid_device) noexcept {
            auto mmios = device::DevResManager::get_mmio_resource(node);
            if (mmios.empty()) {
                if (emit_log) {
                    loggers::DEVICE::DEBUG("virtio 节点 %s 缺少 MMIO 资源",
                                           node.name());
                }
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            auto *mmio = mmios.front().get();
            if (mmio == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (mmio->region().size() < virtio::offset::TOTAL_SIZE) {
                if (emit_log) {
                    loggers::DEVICE::DEBUG(
                        "virtio 节点 %s MMIO 区域过小: size=%llu need=%llu",
                        node.name(),
                        static_cast<unsigned long long>(mmio->region().size()),
                        static_cast<unsigned long long>(
                            virtio::offset::TOTAL_SIZE));
                }
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            auto map_res = device::MMIOManager::inst().map_to_kernel(*mmio);
            propagate(map_res);
            util::Guard unmap_guard([mmio]() {
                auto unmap_res =
                    device::MMIOManager::inst().unmap_from_kernel(*mmio);
                if (!unmap_res.has_value()) {
                    loggers::DEVICE::ERROR(
                        "virtio 探测后解除 MMIO 映射失败: err=%s",
                        to_cstring(unmap_res.error()));
                }
            });

            ProbeInfo transport_info{
                .valid           = false,
                .legacy          = false,
                .magic_value     = 0,
                .version         = 0,
                .device_id       = 0,
                .vendor_id       = 0,
                .status          = 0,
                .device_features = 0,
                .mmio            = mmio,
            };
            TransportMMIO transport(transport_info, *mmio,
                                    map_res.value().as<char>());
            auto info = transport.probe_info();
            info.valid = is_valid_device(info);

            if (emit_log) {
                log_probe_result(node, info);
            }
            if (!accept_invalid_device && !info.valid) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return info;
        }
    }  // namespace
    TransportMMIO::TransportMMIO(ProbeInfo probe_info,
                                 const device::MMIOResource &mmio,
                                 char *mmio_base) noexcept
        : Transport(probe_info),
          _mmio(&mmio),
          _regs(reinterpret_cast<volatile CommonConfig *>(mmio_base)) {
        if (_regs != nullptr) {
            _probe_info.magic_value = _regs->magic_value;
            _probe_info.version     = _regs->version;
            _probe_info.device_id   = _regs->device_id;
            _probe_info.vendor_id   = _regs->vendor_id;
            _probe_info.status      = _regs->status;
            _regs->device_features_sel = 0;
            _probe_info.device_features = _regs->device_features;
            _probe_info.legacy = _probe_info.version == virtio::VERSION_LEGACY;
        }
    }

    u32 TransportMMIO::read_reg32(size_t reg_offset) const noexcept {
        auto *ptr = reinterpret_cast<volatile u32 *>(
            reinterpret_cast<volatile char *>(_regs) + reg_offset);
        return *ptr;
    }

    void TransportMMIO::write_reg32(size_t reg_offset, u32 value) noexcept {
        auto *ptr = reinterpret_cast<volatile u32 *>(
            reinterpret_cast<volatile char *>(_regs) + reg_offset);
        *ptr = value;
    }

    Result<u8> TransportMMIO::read_config_u8(size_t config_offset) const noexcept {
        if (!_probe_info.legacy) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *ptr = reinterpret_cast<volatile u8 *>(
            reinterpret_cast<volatile char *>(_regs) + offset::CONFIG_SPACE +
            config_offset);
        return static_cast<u8>(*ptr);
    }

    Result<u16> TransportMMIO::read_config_u16(
        size_t config_offset) const noexcept {
        if (!_probe_info.legacy) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *ptr = reinterpret_cast<volatile u16 *>(
            reinterpret_cast<volatile char *>(_regs) + offset::CONFIG_SPACE +
            config_offset);
        return static_cast<u16>(*ptr);
    }

    Result<u32> TransportMMIO::read_config_u32(
        size_t config_offset) const noexcept {
        if (!_probe_info.legacy) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *ptr = reinterpret_cast<volatile u32 *>(
            reinterpret_cast<volatile char *>(_regs) + offset::CONFIG_SPACE +
            config_offset);
        return static_cast<u32>(*ptr);
    }

    Result<u64> TransportMMIO::read_config_u64(
        size_t config_offset) const noexcept {
        auto lo_res = read_config_u32(config_offset);
        propagate(lo_res);
        auto hi_res = read_config_u32(config_offset + sizeof(u32));
        propagate(hi_res);
        return (static_cast<u64>(hi_res.value()) << 32) | lo_res.value();
    }

    TransportPCI::TransportPCI(const pci::PCIDeviceNode &node,
                               ProbeInfo probe_info) noexcept
        : Transport(probe_info), _node(&node), _modern(false) {
        auto parse_res = parse_capabilities();
        if (!parse_res.has_value()) {
            return;
        }
        auto probe_res = load_probe_info();
        if (!probe_res.has_value()) {
            return;
        }
    }

    TransportPCI::~TransportPCI() {
        auto unmap_one = [](util::owner<device::MMIOResource *> &resource) {
            if (resource.get() == nullptr || !resource->mapped()) {
                return;
            }
            auto unmap_res =
                device::MMIOManager::inst().unmap_from_kernel(*resource.get());
            if (!unmap_res.has_value()) {
                loggers::DEVICE::ERROR(
                    "解除 virtio-pci MMIO capability 映射失败: err=%s",
                    to_cstring(unmap_res.error()));
            }
        };
        unmap_one(_common_resource);
        unmap_one(_notify_resource);
        unmap_one(_device_resource);
    }

    Result<void> TransportPCI::parse_capabilities() noexcept {
        if (_node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        b16 offset = _node->capability_pointer();
        loggers::DEVICE::INFO(
            "virtio-pci 开始解析 capability 链: node=%s cap_ptr=0x%02x",
            _node->name(), static_cast<unsigned>(offset));
        size_t limit = 64;
        while (offset != 0 && offset < 0x1000 && limit-- > 0) {
            auto cap_id = _node->read_config8(offset);
            loggers::DEVICE::INFO(
                "virtio-pci capability: node=%s offset=0x%02x cap_id=0x%02x next=0x%02x",
                _node->name(), static_cast<unsigned>(offset),
                static_cast<unsigned>(cap_id),
                static_cast<unsigned>(
                    _node->read_config8(static_cast<b16>(offset + 1))));
            if (cap_id == pci_cap::CAP_ID_VENDOR) {
                auto parse_res = parse_vendor_cap(offset);
                propagate(parse_res);
            }
            offset = _node->read_config8(static_cast<b16>(offset + 1));
        }

        if (!_common_cfg.valid || !_notify_cfg.valid || !_device_cfg.valid) {
            loggers::DEVICE::ERROR(
                "virtio-pci capability 不完整: node=%s common=%s notify=%s device=%s",
                _node->name(), _common_cfg.valid ? "yes" : "no",
                _notify_cfg.valid ? "yes" : "no",
                _device_cfg.valid ? "yes" : "no");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto common_map_res = map_bar_region(_common_cfg, _common_resource);
        propagate(common_map_res);
        _common_base = common_map_res.value();

        auto notify_map_res = map_bar_region(_notify_cfg, _notify_resource);
        propagate(notify_map_res);
        _notify_base = notify_map_res.value();

        auto device_map_res = map_bar_region(_device_cfg, _device_resource);
        propagate(device_map_res);
        _device_base = device_map_res.value();

        _modern = true;
        void_return();
    }

    Result<void> TransportPCI::parse_vendor_cap(b16 offset) noexcept {
        auto cfg_type = _node->read_config8(static_cast<b16>(offset + 3));
        auto bar = _node->read_config8(static_cast<b16>(offset + 4));
        auto region_offset =
            _node->read_config32(static_cast<b16>(offset + 8));
        auto region_length =
            _node->read_config32(static_cast<b16>(offset + 12));
        loggers::DEVICE::INFO(
            "virtio-pci vendor cap: node=%s offset=0x%02x cfg_type=%u bar=%u cap_off=0x%x len=0x%x",
            _node->name(), static_cast<unsigned>(offset),
            static_cast<unsigned>(cfg_type), static_cast<unsigned>(bar),
            static_cast<unsigned>(region_offset),
            static_cast<unsigned>(region_length));
        switch (cfg_type) {
            case pci_cap::CFG_COMMON: {
                auto region_res = read_region(offset, pci_cap::CFG_COMMON);
                propagate(region_res);
                _common_cfg = region_res.value();
                loggers::DEVICE::INFO(
                    "virtio-pci common cfg 就绪: node=%s cpu=0x%llx len=0x%x",
                    _node->name(),
                    static_cast<unsigned long long>(_common_cfg.cpu_addr),
                    static_cast<unsigned>(_common_cfg.length));
                break;
            }
            case pci_cap::CFG_NOTIFY: {
                auto region_res = read_region(offset, pci_cap::CFG_NOTIFY);
                propagate(region_res);
                _notify_cfg = region_res.value();
                _notify_off_multiplier =
                    _node->read_config32(static_cast<b16>(offset + 16));
                loggers::DEVICE::INFO(
                    "virtio-pci notify cfg 就绪: node=%s cpu=0x%llx len=0x%x multiplier=0x%x",
                    _node->name(),
                    static_cast<unsigned long long>(_notify_cfg.cpu_addr),
                    static_cast<unsigned>(_notify_cfg.length),
                    static_cast<unsigned>(_notify_off_multiplier));
                break;
            }
            case pci_cap::CFG_DEVICE: {
                auto region_res = read_region(offset, pci_cap::CFG_DEVICE);
                propagate(region_res);
                _device_cfg = region_res.value();
                loggers::DEVICE::INFO(
                    "virtio-pci device cfg 就绪: node=%s cpu=0x%llx len=0x%x",
                    _node->name(),
                    static_cast<unsigned long long>(_device_cfg.cpu_addr),
                    static_cast<unsigned>(_device_cfg.length));
                break;
            }
            default: break;
        }
        void_return();
    }

    Result<VirtioPciRegion> TransportPCI::read_region(
        b16 cap_offset, b8 expected_cfg_type) const noexcept {
        if (_node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto cfg_type = _node->read_config8(static_cast<b16>(cap_offset + 3));
        if (cfg_type != expected_cfg_type) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto bar = _node->read_config8(static_cast<b16>(cap_offset + 4));
        const auto *bar_info = _node->find_bar(bar);
        if (bar_info == nullptr || !bar_info->valid || !bar_info->mmio ||
            bar_info->cpu_addr == 0)
        {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        auto region_offset = _node->read_config32(static_cast<b16>(cap_offset + 8));
        auto region_length = _node->read_config32(static_cast<b16>(cap_offset + 12));
        return VirtioPciRegion{
            .valid = true,
            .bar = bar,
            .cpu_addr = bar_info->cpu_addr + region_offset,
            .length = region_length,
        };
    }

    Result<void> TransportPCI::load_probe_info() noexcept {
        if (_node == nullptr || !_common_cfg.valid || _common_base == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const auto &identity = _node->identity();
        auto *common = reinterpret_cast<volatile VirtioPciCommonCfg *>(
            _common_base);

        u32 device_id = 0;
        if (identity.device_id >= 0x1000 && identity.device_id < 0x1040) {
            device_id = static_cast<u32>(identity.device_id - 0x0FFF);
        } else if (identity.device_id >= 0x1040) {
            device_id = static_cast<u32>(identity.device_id - 0x1040);
        }

        _probe_info.magic_value = MAGIC_VALUE;
        _probe_info.version = VERSION_MODERN;
        _probe_info.device_id = device_id;
        _probe_info.vendor_id = identity.vendor_id;
        _probe_info.status = common->device_status;
        common->device_feature_select = 0;
        const auto lo = common->device_feature;
        common->device_feature_select = 1;
        const auto hi = common->device_feature;
        _probe_info.device_features =
            static_cast<u32>(lo | (hi != 0 ? 0x80000000u : 0u));
        _probe_info.legacy = false;
        _probe_info.valid = is_valid_device(_probe_info);
        void_return();
    }

    Result<volatile char *> TransportPCI::map_bar_region(
        const VirtioPciRegion &region,
        util::owner<device::MMIOResource *> &resource) noexcept {
        if (!region.valid || region.cpu_addr == 0 || region.length == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        resource = device::MMIOResource::make(PhyArea(
            PhyAddr(static_cast<addr_t>(region.cpu_addr)),
            PhyAddr(static_cast<addr_t>(region.cpu_addr + region.length))));
        if (resource.get() == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto map_res = device::MMIOManager::inst().map_to_kernel(*resource.get());
        propagate(map_res);
        return map_res.value().as<char>();
    }

    u32 TransportPCI::read_reg32(size_t reg_offset) const noexcept {
        if (!_common_cfg.valid || _common_base == nullptr) {
            return 0;
        }
        auto mapped = pci_common_reg_offset(reg_offset);
        if (!mapped.has_value()) {
            return 0;
        }
        if (reg_offset == offset::STATUS) {
            auto *ptr = reinterpret_cast<volatile u8 *>(_common_base + *mapped);
            return *ptr;
        }
        if (reg_offset == offset::QUEUE_SEL || reg_offset == offset::QUEUE_NUM ||
            reg_offset == offset::QUEUE_NUM_MAX)
        {
            auto *ptr =
                reinterpret_cast<volatile u16 *>(_common_base + *mapped);
            return *ptr;
        }
        auto *ptr = reinterpret_cast<volatile u32 *>(_common_base + *mapped);
        return *ptr;
    }

    void TransportPCI::write_reg32(size_t reg_offset, u32 value) noexcept {
        if (!_common_cfg.valid || _common_base == nullptr) {
            return;
        }
        auto mapped = pci_common_reg_offset(reg_offset);
        if (!mapped.has_value()) {
            return;
        }
        if (reg_offset == offset::STATUS) {
            auto *ptr = reinterpret_cast<volatile u8 *>(_common_base + *mapped);
            *ptr = static_cast<u8>(value);
            return;
        }
        if (reg_offset == offset::QUEUE_SEL || reg_offset == offset::QUEUE_NUM ||
            reg_offset == offset::QUEUE_NUM_MAX)
        {
            auto *ptr =
                reinterpret_cast<volatile u16 *>(_common_base + *mapped);
            *ptr = static_cast<u16>(value);
            return;
        }
        auto *ptr = reinterpret_cast<volatile u32 *>(_common_base + *mapped);
        *ptr = value;
    }

    Result<u8> TransportPCI::read_config_u8(size_t config_offset) const noexcept {
        if (!_device_cfg.valid || _device_base == nullptr) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *ptr = reinterpret_cast<volatile u8 *>(_device_base + config_offset);
        return static_cast<u8>(*ptr);
    }

    Result<u16> TransportPCI::read_config_u16(
        size_t config_offset) const noexcept {
        if (!_device_cfg.valid || _device_base == nullptr) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *ptr = reinterpret_cast<volatile u16 *>(_device_base + config_offset);
        return static_cast<u16>(*ptr);
    }

    Result<u32> TransportPCI::read_config_u32(
        size_t config_offset) const noexcept {
        if (!_device_cfg.valid || _device_base == nullptr) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *ptr = reinterpret_cast<volatile u32 *>(_device_base + config_offset);
        return static_cast<u32>(*ptr);
    }

    Result<u64> TransportPCI::read_config_u64(
        size_t config_offset) const noexcept {
        auto lo_res = read_config_u32(config_offset);
        propagate(lo_res);
        auto hi_res = read_config_u32(config_offset + sizeof(u32));
        propagate(hi_res);
        return (static_cast<u64>(hi_res.value()) << 32) | lo_res.value();
    }

    Result<u16> TransportPCI::max_queue_size(u16 queue_index) noexcept {
        auto *common = reinterpret_cast<volatile VirtioPciCommonCfg *>(
            _common_base);
        common->queue_select = queue_index;
        return static_cast<u16>(common->queue_size);
    }

    Result<void> TransportPCI::setup_queue(u16 queue_index, u16 queue_size,
                                           PhyAddr desc, PhyAddr driver_area,
                                           PhyAddr device_area) noexcept {
        auto *common = reinterpret_cast<volatile VirtioPciCommonCfg *>(
            _common_base);
        common->queue_select = queue_index;
        common->queue_size = queue_size;
        common->queue_desc = desc.arith();
        common->queue_driver = driver_area.arith();
        common->queue_device = device_area.arith();
        common->queue_enable = 1;
        void_return();
    }

    Result<void> TransportPCI::notify_queue(u16 queue_index) noexcept {
        auto *common = reinterpret_cast<volatile VirtioPciCommonCfg *>(
            _common_base);
        common->queue_select = queue_index;
        const auto notify_off = common->queue_notify_off;
        auto *notify_addr = reinterpret_cast<volatile u16 *>(
            _notify_base +
            static_cast<size_t>(notify_off) * _notify_off_multiplier);
        *notify_addr = queue_index;
        void_return();
    }

    VirtioDriverBase::VirtioDriverBase(DevRes res, ProbeInfo probe_info,
                                       util::owner<Transport *> transport) noexcept
        : DriverBase(std::move(res)),
          _probe_info(probe_info),
          _transport(std::move(transport)),
          _device_features(0),
          _negotiated_features(0),
          _queues() {}

    VirtioDriverBase::~VirtioDriverBase() noexcept {
        // 先释放队列 DMA 区, 再清空运行时视图, 避免析构后悬空访问.
        for (auto &queue : _queues) {
            free_dma_buffer(queue.ring);
        }
        _queues.clear();
    }

    Result<void> VirtioDriverBase::begin_init(u64 supported_features) noexcept {
        // 先校验 transport 前置条件，当前通用运行时支持 legacy-mmio
        // 与 modern-pci 两条路径。
        if (!_probe_info.valid) {
            loggers::DEVICE::ERROR(
                "VirtioDriverBase 仅支持已探测的 virtio 设备: node=%s",
                name());
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        // 然后按 virtio 状态机顺序推进初始化, 并在中途完成 feature 协商.
        set_status(0);
        set_status(device_status::ACKNOWLEDGE | device_status::DRIVER);

        auto features_res = read_device_features();
        propagate(features_res);
        _device_features     = features_res.value();
        _negotiated_features = _device_features & supported_features;

        auto write_res = write_driver_features(_negotiated_features);
        propagate(write_res);

        set_status(device_status::ACKNOWLEDGE | device_status::DRIVER |
                   device_status::FEATURES_OK);
        const auto cur_status = status();
        if ((cur_status & device_status::FEATURES_OK) == 0u) {
            loggers::DEVICE::ERROR(
                "virtio 设备拒绝 feature 协商: node=%s features=0x%llx", name(),
                static_cast<unsigned long long>(_negotiated_features));
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        loggers::DEVICE::DEBUG(
            "virtio 通用初始化完成: node=%s device_features=0x%llx "
            "negotiated=0x%llx",
            name(), static_cast<unsigned long long>(_device_features),
            static_cast<unsigned long long>(_negotiated_features));
        void_return();
    }

    Result<void> VirtioDriverBase::finish_init() noexcept {
        const auto new_status = status() | device_status::DRIVER_OK;
        set_status(new_status);
        loggers::DEVICE::DEBUG("virtio 设备进入 DRIVER_OK: node=%s status=0x%x",
                               name(), static_cast<unsigned>(new_status));
        void_return();
    }

    Result<u8> VirtioDriverBase::read_config_u8(
        size_t config_offset) const noexcept {
        if (_transport.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _transport->read_config_u8(config_offset);
    }

    Result<u16> VirtioDriverBase::read_config_u16(
        size_t config_offset) const noexcept {
        if (_transport.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _transport->read_config_u16(config_offset);
    }

    Result<u32> VirtioDriverBase::read_config_u32(
        size_t config_offset) const noexcept {
        if (_transport.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _transport->read_config_u32(config_offset);
    }

    Result<u64> VirtioDriverBase::read_config_u64(
        size_t config_offset) const noexcept {
        if (_transport.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _transport->read_config_u64(config_offset);
    }

    Result<VirtQueueLegacy *> VirtioDriverBase::init_queue_legacy(
        u16 queue_index, u16 requested_size) noexcept {
        // 先读取设备给出的队列上限, 再以驱动本地上限裁剪最终队列大小.
        if (!_probe_info.legacy) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        write_reg32(offset::QUEUE_SEL, queue_index);
        const auto max_size = read_reg32(offset::QUEUE_NUM_MAX);
        if (max_size == 0) {
            loggers::DEVICE::ERROR("virtio 队列 %u 不可用", queue_index);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const auto queue_size = static_cast<u16>(std::min<u32>(
            std::min<u32>(requested_size, MAX_QUEUE_SIZE), max_size));
        size_t avail_offset   = 0;
        size_t used_offset    = 0;
        const auto total_bytes =
            queue_total_bytes(queue_size, &avail_offset, &used_offset);

        // 然后分配一段连续 DMA 区, 在其中按 legacy 布局拼出 desc/avail/used.
        auto ring_res = alloc_dma_buffer(total_bytes);
        propagate(ring_res);
        auto ring = ring_res.value();
        std::memset(ring.kaddr, 0, ring.size);

        auto *base = static_cast<char *>(ring.kaddr);
        VirtQueueLegacy queue{
            .queue_index = queue_index,
            .size        = queue_size,
            .ring        = ring,
            .desc        = reinterpret_cast<VirtQueueDescLegacy *>(base),
            .avail_flags =
                reinterpret_cast<volatile le16 *>(base + avail_offset),
            .avail_index = reinterpret_cast<volatile le16 *>(
                base + avail_offset + sizeof(le16)),
            .avail_ring = reinterpret_cast<volatile le16 *>(
                base + avail_offset + sizeof(le16) * 2),
            .used_flags = reinterpret_cast<volatile le16 *>(base + used_offset),
            .used_index = reinterpret_cast<volatile le16 *>(base + used_offset +
                                                            sizeof(le16)),
            .used_ring  = reinterpret_cast<volatile VirtQueueUsedElemLegacy *>(
                base + used_offset + sizeof(le16) * 2),
            .avail_offset     = avail_offset,
            .used_offset      = used_offset,
            .ring_bytes       = total_bytes,
            .free_head        = 0,
            .free_count       = queue_size,
            .last_used_index  = 0,
            .last_avail_index = 0,
        };

        // 最后构造自由描述符链并把 PFN 写回设备, 使队列对设备可见.
        for (u16 i = 0; i < queue_size; ++i) {
            queue.desc[i].next = (i + 1 < queue_size)
                                     ? static_cast<le16>(i + 1)
                                     : static_cast<le16>(vring::INVALID_DESC);
        }

        write_reg32(offset::QUEUE_ALIGN, LEGACY_QUEUE_ALIGNMENT);
        write_reg32(offset::GUEST_PAGE_SIZE, PAGESIZE);
        write_reg32(offset::QUEUE_NUM, queue_size);
        write_reg32(offset::QUEUE_PFN,
                    static_cast<u32>(ring.paddr.arith() / PAGESIZE));

        _queues.push_back(queue);
        auto *created = &_queues.back();
        loggers::DEVICE::DEBUG(
            "virtio 队列初始化完成: node=%s queue=%u size=%u paddr=%p "
            "bytes=%llu",
            name(), static_cast<unsigned>(queue_index),
            static_cast<unsigned>(queue_size), created->ring.paddr.addr(),
            static_cast<unsigned long long>(created->ring.size));
        return created;
    }

    Result<VirtQueueLegacy *> VirtioDriverBase::init_queue_modern(
        u16 queue_index, u16 requested_size) noexcept {
        auto *transport = _transport.get();
        if (transport == nullptr || transport->kind() != Transport::Kind::PCI) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto *transport_pci = static_cast<TransportPCI *>(transport);
        if (!transport_pci->modern()) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        auto max_size_res = transport_pci->max_queue_size(queue_index);
        propagate(max_size_res);
        const auto queue_size = static_cast<u16>(std::min<u32>(
            std::min<u32>(requested_size, MAX_QUEUE_SIZE), max_size_res.value()));
        if (queue_size == 0) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        size_t avail_offset = 0;
        size_t used_offset = 0;
        const auto total_bytes =
            queue_total_bytes(queue_size, &avail_offset, &used_offset);
        auto ring_res = alloc_dma_buffer(total_bytes);
        propagate(ring_res);
        auto ring = ring_res.value();
        std::memset(ring.kaddr, 0, ring.size);

        auto *base = static_cast<char *>(ring.kaddr);
        VirtQueueLegacy queue{
            .queue_index = queue_index,
            .size        = queue_size,
            .ring        = ring,
            .desc        = reinterpret_cast<VirtQueueDescLegacy *>(base),
            .avail_flags =
                reinterpret_cast<volatile le16 *>(base + avail_offset),
            .avail_index = reinterpret_cast<volatile le16 *>(
                base + avail_offset + sizeof(le16)),
            .avail_ring = reinterpret_cast<volatile le16 *>(
                base + avail_offset + sizeof(le16) * 2),
            .used_flags = reinterpret_cast<volatile le16 *>(base + used_offset),
            .used_index = reinterpret_cast<volatile le16 *>(base + used_offset +
                                                            sizeof(le16)),
            .used_ring  = reinterpret_cast<volatile VirtQueueUsedElemLegacy *>(
                base + used_offset + sizeof(le16) * 2),
            .avail_offset     = avail_offset,
            .used_offset      = used_offset,
            .ring_bytes       = total_bytes,
            .free_head        = 0,
            .free_count       = queue_size,
            .last_used_index  = 0,
            .last_avail_index = 0,
        };

        for (u16 i = 0; i < queue_size; ++i) {
            queue.desc[i].next = (i + 1 < queue_size)
                                     ? static_cast<le16>(i + 1)
                                     : static_cast<le16>(vring::INVALID_DESC);
        }

        auto desc_paddr = ring.paddr;
        auto driver_paddr = ring.paddr + avail_offset;
        auto device_paddr = ring.paddr + used_offset;
        auto setup_res = transport_pci->setup_queue(queue_index, queue_size,
                                                    desc_paddr, driver_paddr,
                                                    device_paddr);
        propagate(setup_res);

        _queues.push_back(queue);
        return &_queues.back();
    }

    Result<u16> VirtioDriverBase::queue_add_chain_legacy(
        VirtQueueLegacy &queue,
        const std::vector<vring::BufferView> &buffers) noexcept {
        // 该函数只负责“预留并构造”描述符链, 真正入队由 submit 路径完成.
        if (buffers.empty() || buffers.size() > queue.free_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        std::vector<u16> desc_indices;
        desc_indices.reserve(buffers.size());
        u16 current = queue.free_head;
        for (size_t i = 0; i < buffers.size(); ++i) {
            if (current == vring::INVALID_DESC) {
                unexpect_return(ErrCode::NO_FREE_SLOT);
            }
            desc_indices.push_back(current);
            current = queue.desc[current].next;
        }

        queue.free_head = current;
        queue.free_count =
            static_cast<u16>(queue.free_count - desc_indices.size());

        for (size_t i = 0; i < buffers.size(); ++i) {
            const auto idx     = desc_indices[i];
            const auto &buffer = buffers[i];
            auto flags         = static_cast<u16>(0);
            if (buffer.writable) {
                flags |= vring::desc_flag::WRITE;
            }
            if (i + 1 < buffers.size()) {
                flags                |= vring::desc_flag::NEXT;
                queue.desc[idx].next  = desc_indices[i + 1];
            } else {
                queue.desc[idx].next = vring::INVALID_DESC;
            }
            queue.desc[idx].addr  = buffer.paddr.arith();
            queue.desc[idx].len   = static_cast<le32>(buffer.size);
            queue.desc[idx].flags = flags;
        }
        return desc_indices.front();
    }

    Result<void> VirtioDriverBase::queue_submit_legacy(
        VirtQueueLegacy &queue, u16 head_desc) noexcept {
        queue.avail_ring[queue.last_avail_index % queue.size] = head_desc;
        queue.last_avail_index = static_cast<u16>(queue.last_avail_index + 1);
        *queue.avail_index     = queue.last_avail_index;
        void_return();
    }

    Result<void> VirtioDriverBase::queue_notify_legacy(
        VirtQueueLegacy &queue) noexcept {
        auto *transport = _transport.get();
        if (transport != nullptr && transport->kind() == Transport::Kind::PCI) {
            auto *transport_pci = static_cast<TransportPCI *>(transport);
            return transport_pci->notify_queue(queue.queue_index);
        }
        write_reg32(offset::QUEUE_NOTIFY, queue.queue_index);
        void_return();
    }

    Result<bool> VirtioDriverBase::queue_can_pop_legacy(
        VirtQueueLegacy &queue) noexcept {
        return queue.last_used_index != *queue.used_index;
    }

    Result<VirtQueueUsedElemLegacy> VirtioDriverBase::queue_pop_used_legacy(
        VirtQueueLegacy &queue) noexcept {
        if (queue.last_used_index == *queue.used_index) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        const auto used_pos = queue.last_used_index % queue.size;
        VirtQueueUsedElemLegacy elem{
            .id  = queue.used_ring[used_pos].id,
            .len = queue.used_ring[used_pos].len,
        };
        queue.last_used_index = static_cast<u16>(queue.last_used_index + 1);
        return elem;
    }

    Result<void> VirtioDriverBase::queue_free_chain_legacy(
        VirtQueueLegacy &queue, u16 head_desc) noexcept {
        if (head_desc == vring::INVALID_DESC) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        u16 current  = head_desc;
        u16 tail     = head_desc;
        size_t count = 0;
        while (current != vring::INVALID_DESC) {
            ++count;
            tail = current;
            if ((queue.desc[current].flags & vring::desc_flag::NEXT) == 0u) {
                current = vring::INVALID_DESC;
            } else {
                current = queue.desc[current].next;
            }
        }

        queue.desc[tail].next = queue.free_head;
        queue.free_head       = head_desc;
        queue.free_count      = static_cast<u16>(queue.free_count + count);
        void_return();
    }

    Result<VirtQueueUsedElemLegacy>
    VirtioDriverBase::queue_submit_and_poll_legacy(
        VirtQueueLegacy &queue,
        const std::vector<vring::BufferView> &buffers) noexcept {
        // 先构造并发布请求.
        auto head_res = queue_add_chain_legacy(queue, buffers);
        propagate(head_res);
        const auto head_desc = head_res.value();

        auto submit_res = queue_submit_legacy(queue, head_desc);
        if (!submit_res.has_value()) {
            auto free_res = queue_free_chain_legacy(queue, head_desc);
            (void)free_res;
            propagate_return(submit_res);
        }
        auto notify_res = queue_notify_legacy(queue);
        if (!notify_res.has_value()) {
            auto free_res = queue_free_chain_legacy(queue, head_desc);
            (void)free_res;
            propagate_return(notify_res);
        }

        // 然后轮询 used ring. 当前版本不依赖 IRQ, 只在固定轮询次数后主动让出.
        size_t poll_count = 0;
        for (;;) {
            auto can_pop_res = queue_can_pop_legacy(queue);
            propagate(can_pop_res);
            if (can_pop_res.value()) {
                auto used_res = queue_pop_used_legacy(queue);
                if (!used_res.has_value()) {
                    auto free_res = queue_free_chain_legacy(queue, head_desc);
                    (void)free_res;
                    propagate_return(used_res);
                }
                auto free_res = queue_free_chain_legacy(
                    queue, static_cast<u16>(used_res.value().id));
                if (!free_res.has_value()) {
                    propagate_return(free_res);
                }
                return used_res.value();
            }

            ++poll_count;
            if (poll_count % POLL_RETRY_BEFORE_YIELD == 0) {
                schd::Scheduler::inst().yield();
            }
        }
    }

    Result<DmaBuffer> VirtioDriverBase::alloc_dma_buffer(size_t size) noexcept {
        const auto page_count = page_align_up(size) / PAGESIZE;
        auto page_res         = GFP::get_free_page(page_count);
        propagate(page_res);
        auto paddr = page_res.value();
        auto kaddr = convert<KpaAddr>(paddr).addr();
        return DmaBuffer{
            .kaddr    = kaddr,
            .paddr    = paddr,
            .size     = page_count * PAGESIZE,
            .page_cnt = page_count,
        };
    }

    void VirtioDriverBase::free_dma_buffer(DmaBuffer &buffer) noexcept {
        if (!buffer.paddr.nonnull() || buffer.page_cnt == 0) {
            buffer = {};
            return;
        }
        GFP::put_page(buffer.paddr, buffer.page_cnt);
        buffer = {};
    }

    u32 VirtioDriverBase::read_reg32(size_t reg_offset) const noexcept {
        assert(_transport.get() != nullptr);
        return _transport->read_reg32(reg_offset);
    }

    void VirtioDriverBase::write_reg32(size_t reg_offset, u32 value) noexcept {
        assert(_transport.get() != nullptr);
        _transport->write_reg32(reg_offset, value);
    }

    u32 VirtioDriverBase::status() const noexcept {
        return read_reg32(offset::STATUS);
    }

    void VirtioDriverBase::set_status(u32 status_value) noexcept {
        write_reg32(offset::STATUS, status_value);
    }

    Result<u64> VirtioDriverBase::read_device_features() noexcept {
        write_reg32(offset::DEVICE_FEATURE_SEL, 0);
        const auto lo = read_reg32(offset::DEVICE_FEATURE);
        write_reg32(offset::DEVICE_FEATURE_SEL, 1);
        const auto hi = read_reg32(offset::DEVICE_FEATURE);
        return (static_cast<u64>(hi) << 32) | lo;
    }

    Result<void> VirtioDriverBase::write_driver_features(
        u64 features) noexcept {
        write_reg32(offset::DRIVER_FEATURE_SEL, 0);
        write_reg32(offset::DRIVER_FEATURE,
                    static_cast<u32>(features & 0xFFFF'FFFFull));
        write_reg32(offset::DRIVER_FEATURE_SEL, 1);
        write_reg32(offset::DRIVER_FEATURE, static_cast<u32>(features >> 32));
        void_return();
    }

    bool register_factory(const IVirtioDeviceFactory &factory) noexcept {
        auto it = std::ranges::find(g_virtio_factories, &factory);
        if (it != g_virtio_factories.end()) {
            loggers::DEVICE::WARN("virtio 子工厂重复注册: type=%u",
                                  static_cast<unsigned>(factory.device_type()));
            return false;
        }
        g_virtio_factories.push_back(&factory);
        loggers::DEVICE::DEBUG("注册 virtio 子工厂: type=%u",
                               static_cast<unsigned>(factory.device_type()));
        return true;
    }

    bool is_virtio_magic(u32 magic_value) noexcept {
        return magic_value == MAGIC_VALUE;
    }

    bool is_supported_version(u32 version) noexcept {
        return version == VERSION_LEGACY || version == VERSION_MODERN;
    }

    const char *transport_version_name(u32 version) noexcept {
        switch (version) {
            case VERSION_LEGACY: return "legacy";
            case VERSION_MODERN: return "modern";
            default:             return "unknown";
        }
    }

    const char *device_type_name(u32 device_id) noexcept {
        switch (static_cast<DeviceType>(device_id)) {
            case DeviceType::INVALID:        return "invalid";
            case DeviceType::NETWORK:        return "network";
            case DeviceType::BLOCK:          return "block";
            case DeviceType::CONSOLE:        return "console";
            case DeviceType::ENTROPY:        return "entropy";
            case DeviceType::MEMORY_BALLOON: return "memory-balloon";
            case DeviceType::IOMEM:          return "iomem";
            case DeviceType::RPMSG:          return "rpmsg";
            case DeviceType::SCSI_HOST:      return "scsi-host";
            case DeviceType::TRANSPORT_9P:   return "9p";
            default:                         return "unknown";
        }
    }

    const char *device_status_name(u32 status) noexcept {
        if (status == 0) {
            return "reset";
        }
        if ((status & device_status::FAILED) != 0u) {
            return "failed";
        }
        if ((status & device_status::NEEDS_RESET) != 0u) {
            return "needs-reset";
        }
        if ((status & device_status::DRIVER_OK) != 0u) {
            return "driver-ok";
        }
        if ((status & device_status::FEATURES_OK) != 0u) {
            return "features-ok";
        }
        if ((status & device_status::DRIVER) != 0u) {
            return "driver";
        }
        if ((status & device_status::ACKNOWLEDGE) != 0u) {
            return "acknowledge";
        }
        return "unknown";
    }

    Result<ProbeInfo> probe_mmio_device(
        const device::DeviceNode &node) noexcept {
        return probe_mmio_device_impl(node, true, true);
    }

    Result<ProbeInfo> probe_pci_device(const device::DeviceNode &node) noexcept {
        auto *pci_node = node.as<pci::PCIDeviceNode>();
        if (pci_node == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        ProbeInfo info{};
        auto transport = TransportPCI(*pci_node, info);
        info = transport.probe_info();
        if (!info.valid) {
            loggers::DEVICE::ERROR(
                "virtio-pci 探测失败或当前仅检测到 legacy transport: node=%s",
                node.name());
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        log_probe_result(node, info);
        return info;
    }

    const driver::DeviceId &VirtioMmioFactory::device_id() const noexcept {
        return VIRTIO_MMIO_DEVICE_ID;
    }

    bool VirtioMmioFactory::probe(const device::DeviceNode &node,
                                  device::DeviceModel &model,
                                  b64 driver_flag) const noexcept {
        (void)model;
        (void)driver_flag;
        if (node.platform() == device::DevicePlatform::PCI) {
            auto probe_res = probe_pci_device(node);
            return probe_res.has_value() && is_valid_device(probe_res.value());
        }
        auto probe_res = probe_mmio_device_impl(node, true, true);
        return probe_res.has_value() && is_valid_device(probe_res.value());
    }

    Result<driver::DriverBase *> VirtioMmioFactory::create(
        const device::DeviceNode &node, device::DeviceModel &model,
        b64 driver_flag) const {
        (void)driver_flag;
        if (node.platform() == device::DevicePlatform::PCI) {
            auto probe_res = probe_pci_device(node);
            propagate(probe_res);
            const auto &info = probe_res.value();
            auto *factory = find_virtio_factory(node, model, info);
            if (factory == nullptr) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return factory->create(node, model, info);
        }
        auto probe_res = probe_mmio_device(node);
        propagate(probe_res);
        const auto &info = probe_res.value();
        if (!is_valid_device(info)) {
            loggers::DEVICE::ERROR("virtio 设备非法，拒绝创建驱动: node=%s",
                                   node.name());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *factory = find_virtio_factory(node, model, info);
        if (factory == nullptr) {
            loggers::DEVICE::DEBUG(
                "virtio 设备已识别但暂无子驱动接管: node=%s type=%s(%u)",
                node.name(), device_type_name(info.device_id),
                static_cast<unsigned>(info.device_id));
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return factory->create(node, model, info);
    }
}  // namespace virtio
