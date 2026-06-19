/**
 * @file device_node.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 统一设备节点适配实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/fdt/decode.h>
#include <device/fdt/device_node.h>

#include <cstring>

namespace fdt {
    DevicePropView::DevicePropView(PropType type, const byte *data,
                                   size_t size) noexcept
        : _type(type), _data(data), _size(size) {}

    DevicePropView DevicePropView::from_region_list(
        std::vector<PhyArea> regions) noexcept {
        DevicePropView view;
        view._type    = PropType::REGION_LIST;
        view._regions = std::move(regions);
        return view;
    }

    DevicePropView::PropType DevicePropView::type() const noexcept {
        return _type;
    }

    std::vector<byte> DevicePropView::raw_bytes() const {
        if (_data == nullptr || _size == 0) {
            return {};
        }
        return {_data, _data + _size};
    }

    std::vector<byte> DevicePropView::as_byte_array() const {
        return raw_bytes();
    }

    std::string_view DevicePropView::as_string() const {
        if (_data == nullptr || _size == 0) {
            return {};
        }
        size_t len = 0;
        while (len < _size && _data[len] != '\0') {
            ++len;
        }
        return std::string_view(reinterpret_cast<const char *>(_data), len);
    }

    std::vector<std::string_view> DevicePropView::as_string_list() const {
        std::vector<std::string_view> result;
        if (_data == nullptr || _size == 0) {
            return result;
        }

        size_t offset = 0;
        while (offset < _size) {
            const auto *base =
                reinterpret_cast<const char *>(_data + offset);
            size_t len = 0;
            while (offset + len < _size && base[len] != '\0') {
                ++len;
            }
            if (len != 0) {
                result.emplace_back(base, len);
            }
            offset += len + 1;
        }
        return result;
    }

    sus_u64 DevicePropView::as_integer(size_t cellsz) const {
        if (_data == nullptr || _size != cellsz || cellsz == 0 ||
            cellsz > sizeof(sus_u64))
        {
            assert(false && "invalid integer property");
            return 0;
        }
        return parse_be_integer(_data, cellsz);
    }

    std::vector<sus_u64> DevicePropView::as_integer_list(size_t cellsz) const {
        std::vector<sus_u64> result;
        if (_data == nullptr || cellsz == 0 || _size % cellsz != 0 ||
            cellsz > sizeof(sus_u64))
        {
            assert(false && "invalid integer-list property");
            return result;
        }

        result.reserve(_size / cellsz);
        for (size_t offset = 0; offset < _size; offset += cellsz) {
            result.push_back(parse_be_integer(_data + offset, cellsz));
        }
        return result;
    }

    std::vector<PhyArea> DevicePropView::as_region_list() const {
        return _regions;
    }

    uint64_t DevicePropView::parse_be_integer(const byte *data,
                                              size_t size) noexcept {
        uint64_t value = 0;
        if (data == nullptr || size == 0 || size > sizeof(uint64_t)) {
            return 0;
        }
        for (size_t i = 0; i < size; ++i) {
            value = (value << 8) | data[i];
        }
        return value;
    }

    FDTDeviceNode::FDTDeviceNode(const FDTProvider &provider,
                                 const Configuration &config,
                                 const Node &node) noexcept
        : _provider(&provider), _config(&config), _node(&node) {}

    const char *FDTDeviceNode::name() const noexcept {
        if (_node == nullptr) {
            return "unknown";
        }
        return _node->name.c_str();
    }

    Optional<DevicePropView> FDTDeviceNode::property(
        const std::string_view &name) const noexcept {
        if (_node == nullptr || _provider == nullptr || _config == nullptr) {
            return std::nullopt;
        }
        return raw_property(std::string(name).c_str());
    }

    Optional<DevicePropView> FDTDeviceNode::raw_property(
        const char *prop_name) const noexcept {
        if (_node == nullptr || prop_name == nullptr) {
            return std::nullopt;
        }

        auto prop_it = _node->properties.find(prop_name);
        if (prop_it == _node->properties.end()) {
            return std::nullopt;
        }

        const auto &prop = *prop_it->second;
        DevicePropView::PropType type = DevicePropView::PropType::ANY;
        if (prop.size == 0) {
            type = DevicePropView::PropType::NONE;
        } else if (std::strcmp(prop_name, COMPATIBLE_PROP) == 0 ||
                   std::strcmp(prop_name, STATUS_PROP) == 0 ||
                   std::strcmp(prop_name, DEVICE_TYPE_PROP) == 0)
        {
            auto strings = prop.as_string_list();
            type = strings.size() <= 1 ? DevicePropView::PropType::STRING
                                       : DevicePropView::PropType::STRING_LIST;
        } else if (prop.size == sizeof(sus_u32)) {
            type = DevicePropView::PropType::INTEGER;
        } else if (prop.size % sizeof(sus_u32) == 0) {
            type = DevicePropView::PropType::INTEGER_LIST;
        } else {
            type = DevicePropView::PropType::BYTE_ARRAY;
        }

        return DevicePropView(type, reinterpret_cast<const byte *>(prop.data),
                              prop.size);
    }

    std::vector<std::string_view> FDTDeviceNode::compatibles() const noexcept {
        auto prop_res = property(COMPATIBLE_PROP);
        if (!prop_res.has_value()) {
            return {};
        }
        return prop_res->as_string_list();
    }

    int FDTDeviceNode::is_compatible_with(
        std::string_view compatible) const noexcept {
        auto _compatibles = compatibles();
        auto it           = std::find(_compatibles.begin(), _compatibles.end(),
                                      compatible);
        if (it == _compatibles.end()) {
            return -1;
        }
        return static_cast<int>(std::distance(_compatibles.begin(), it));
    }

    std::vector<PhyArea> FDTDeviceNode::mmio_regions() const noexcept {
        if (_node == nullptr) {
            return {};
        }

        auto reg_it = _node->properties.find(REG_PROP);
        if (reg_it == _node->properties.end()) {
            loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 缺少 reg 属性",
                                   _node->name.c_str());
            return {};
        }

        auto regions = reg_it->second->as_regions(node_region_cells(*_node));
        loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 解析 mmio 区域数=%u",
                               _node->name.c_str(),
                               static_cast<unsigned>(regions.size()));
        return regions;
    }

    std::vector<device::RawIrqSpec> FDTDeviceNode::irq_specs() const noexcept {
        if (_node == nullptr || _provider == nullptr) {
            return {};
        }
        if (!_node->properties.contains(INTERRUPT_EXT_PROP) &&
            !_node->properties.contains(INTERRUPTS_PROP))
        {
            loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 未声明 irqs",
                                   _node->name.c_str());
            return {};
        }

        auto specs_res = _provider->parse_interrupt_specs(*_node);
        if (!specs_res.has_value()) {
            if (specs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                loggers::DEVICE::ERROR(
                    "FDTDeviceNode[%s] 解析 irq specs 失败: %s",
                    _node->name.c_str(), to_cstring(specs_res.error()));
            }
            return {};
        }
        return specs_res.value();
    }
}  // namespace fdt
