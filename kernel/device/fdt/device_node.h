/**
 * @file device_node.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 统一设备节点适配
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/fdt/provider.h>

namespace fdt {
    /**
     * @brief FDT 属性值的轻量视图.
     *
     * 仅供 FDT 后端与依赖原始 FDT 属性的 FDT 驱动工厂使用。
     */
    class DevicePropView {
    public:
        enum class PropType {
            NONE,
            BYTE_ARRAY,
            STRING,
            STRING_LIST,
            INTEGER,
            INTEGER_LIST,
            ANY,
            REGION_LIST,
        };

        DevicePropView() noexcept = default;
        DevicePropView(PropType type, const byte *data, size_t size) noexcept;

        [[nodiscard]]
        static DevicePropView from_region_list(
            std::vector<PhyArea> regions) noexcept;

        ~DevicePropView() = default;

        [[nodiscard]]
        PropType type() const noexcept;
        [[nodiscard]]
        std::vector<byte> raw_bytes() const;
        [[nodiscard]]
        std::vector<byte> as_byte_array() const;
        [[nodiscard]]
        std::string_view as_string() const;
        [[nodiscard]]
        std::vector<std::string_view> as_string_list() const;
        [[nodiscard]]
        sus_u64 as_integer(size_t cellsz) const;
        [[nodiscard]]
        std::vector<sus_u64> as_integer_list(size_t cellsz) const;
        [[nodiscard]]
        std::vector<PhyArea> as_region_list() const;

    private:
        [[nodiscard]]
        static uint64_t parse_be_integer(const byte *data,
                                         size_t size) noexcept;

        PropType _type    = PropType::NONE;
        const byte *_data = nullptr;
        size_t _size      = 0;
        std::vector<PhyArea> _regions;
    };

    class FDTDeviceNode final : public device::DeviceNode {
    public:
        static constexpr device::DevicePlatform IDENTIFIER =
            device::DevicePlatform::FDT;

        FDTDeviceNode(const FDTProvider &provider, const Configuration &config,
                      const Node &node) noexcept;
        ~FDTDeviceNode() override = default;

        [[nodiscard]]
        const char *name() const noexcept override;

        [[nodiscard]]
        std::vector<PhyArea> mmio_regions() const noexcept override;

        [[nodiscard]]
        std::vector<device::RawIrqSpec> irq_specs() const noexcept override;

        [[nodiscard]]
        Optional<DevicePropView> property(
            const std::string_view &name) const noexcept;

        [[nodiscard]]
        std::vector<std::string_view> compatibles() const noexcept;

        [[nodiscard]]
        int is_compatible_with(std::string_view compatible) const noexcept;

        [[nodiscard]]
        const Node &raw_node() const noexcept {
            return *_node;
        }

    protected:
        [[nodiscard]]
        device::DevicePlatform type_id() const override {
            return IDENTIFIER;
        }

    private:
        [[nodiscard]]
        Optional<DevicePropView> raw_property(
            const char *prop_name) const noexcept;

        const FDTProvider *_provider = nullptr;
        const Configuration *_config = nullptr;
        const Node *_node            = nullptr;
    };
}  // namespace fdt
