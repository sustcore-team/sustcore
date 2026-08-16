/**
 * @file fdt.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 仅把已校验 FDT 转换为启动期设备目录，不取得硬件资源。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/context.h>
#include <device/fdt.h>
#include <libfdt.h>
#include <log.h>

#include <cstddef>

namespace device::fdt {
    namespace detail {
        constexpr u64_t FDT_NAMESPACE_PHANDLE = 1;
        constexpr u64_t FDT_NAMESPACE_OFFSET  = 2;

        [[nodiscard]] constexpr u32_t node_offset(int node) noexcept {
            return node < 0 ? u32_t(-1) : static_cast<u32_t>(node);
        }

        void copy_text(char *destination, size_t capacity, const char *source,
                       size_t length) noexcept {
            if (capacity == 0)
                return;
            size_t index = 0;
            while (index + 1 < capacity && index < length && source[index] != '\0') {
                destination[index] = source[index];
                ++index;
            }
            destination[index] = '\0';
        }

        [[nodiscard]] bool property_string_equals(const void *value, int length,
                                                  const char *expected) noexcept {
            if (value == nullptr || length <= 0)
                return false;
            const auto *text = static_cast<const char *>(value);
            size_t index     = 0;
            while (expected[index] != '\0') {
                if (index >= static_cast<size_t>(length) || text[index] != expected[index])
                    return false;
                ++index;
            }
            return index < static_cast<size_t>(length) && text[index] == '\0';
        }

        [[nodiscard]] bool node_enabled(const void *tree, int node) noexcept {
            int length         = 0;
            const void *status = fdt_getprop(tree, node, "status", &length);
            return status == nullptr || property_string_equals(status, length, "ok") ||
                   property_string_equals(status, length, "okay");
        }

        [[nodiscard]] bool read_u32_property(const void *tree, int node, const char *name,
                                             u32_t &result) noexcept {
            int length = 0;
            const auto *value =
                static_cast<const fdt32_t *>(fdt_getprop(tree, node, name, &length));
            if (value == nullptr || length != static_cast<int>(sizeof(fdt32_t)))
                return false;
            result = fdt32_to_cpu(*value);
            return true;
        }

        [[nodiscard]] bool read_cells(const fdt32_t *cells, size_t cell_count,
                                      u64_t &result) noexcept {
            if (cells == nullptr || cell_count == 0 || cell_count > 2)
                return false;
            result = 0;
            for (size_t index = 0; index < cell_count; ++index)
                result = (result << 32) | fdt32_to_cpu(cells[index]);
            return true;
        }

        [[nodiscard]] FirmwareId node_id(const void *tree, int node) noexcept {
            const u32_t phandle = fdt_get_phandle(tree, node);
            if (phandle != 0)
                return FirmwareId{.kind         = FirmwareKind::FDT,
                                  .namespace_id = FDT_NAMESPACE_PHANDLE,
                                  .local_id     = phandle};
            return FirmwareId{.kind         = FirmwareKind::FDT,
                              .namespace_id = FDT_NAMESPACE_OFFSET,
                              .local_id     = static_cast<u64_t>(static_cast<u32_t>(node))};
        }

        [[nodiscard]] bool node_has_property(const void *tree, int node,
                                             const char *name) noexcept {
            return fdt_getprop(tree, node, name, nullptr) != nullptr;
        }

        [[nodiscard]] bool node_compatible_with(const void *tree, int node,
                                                const char *compatible) noexcept {
            return fdt_node_check_compatible(tree, node, compatible) == 0;
        }

        [[nodiscard]] DeviceClass classify_device(const void *tree, int node) noexcept {
            if (node_has_property(tree, node, "interrupt-controller"))
                return DeviceClass::INTERRUPT_CONTROLLER;
            if (node_compatible_with(tree, node, "google,goldfish-rtc"))
                return DeviceClass::RTC;
            return DeviceClass::GENERIC;
        }

        [[nodiscard]] BindingPolicy binding_policy(DeviceClass device_class) noexcept {
            switch (device_class) {
                case DeviceClass::INTERRUPT_CONTROLLER: return BindingPolicy::KERNEL_REQUIRED;
                case DeviceClass::RTC:                  return BindingPolicy::KERNEL_PREFERRED;
                case DeviceClass::TIMER:                return BindingPolicy::KERNEL_REQUIRED;
                case DeviceClass::GENERIC:              return BindingPolicy::USER_PREFERRED;
            }
            return BindingPolicy::USER_PREFERRED;
        }

        [[nodiscard]] MmioResource first_reg(const void *tree, int node) noexcept {
            const int parent = fdt_parent_offset(tree, node);
            if (parent < 0)
                return {};

            // ranges 的非空转换尚未进入第一阶段；暂不把 bus address 冒充为物理地址。
            int ranges_length = 0;
            if (fdt_getprop(tree, parent, "ranges", &ranges_length) != nullptr &&
                ranges_length != 0)
            {
                return {};
            }

            u32_t address_cells = 0;
            u32_t size_cells    = 0;
            if (!read_u32_property(tree, parent, "#address-cells", address_cells) ||
                !read_u32_property(tree, parent, "#size-cells", size_cells) || address_cells == 0 ||
                address_cells > 2 || size_cells == 0 || size_cells > 2)
            {
                return {};
            }

            int length      = 0;
            const auto *reg = static_cast<const fdt32_t *>(fdt_getprop(tree, node, "reg", &length));
            const size_t cells_per_region = static_cast<size_t>(address_cells + size_cells);
            if (reg == nullptr || length < static_cast<int>(cells_per_region * sizeof(fdt32_t)))
                return {};

            u64_t begin = 0;
            u64_t size  = 0;
            if (!read_cells(reg, address_cells, begin) ||
                !read_cells(reg + address_cells, size_cells, size) || size == 0 ||
                begin > static_cast<u64_t>(-1) - size)
            {
                return {};
            }
            return MmioResource{
                .area    = PhyArea{PhyAddr(static_cast<addr_t>(begin)),
                                PhyAddr(static_cast<addr_t>(begin + size))},
                .present = true,
            };
        }

        void copy_node_name(const void *tree, int node, char *destination,
                            size_t capacity) noexcept {
            int length       = 0;
            const char *name = fdt_get_name(tree, node, &length);
            if (name != nullptr && length > 0)
                copy_text(destination, capacity, name, static_cast<size_t>(length));
        }

        void copy_first_compatible(const void *tree, int node, char *destination,
                                   size_t capacity) noexcept {
            int length = 0;
            const char *compatible =
                static_cast<const char *>(fdt_getprop(tree, node, "compatible", &length));
            if (compatible != nullptr && length > 0)
                copy_text(destination, capacity, compatible, static_cast<size_t>(length));
        }

        [[nodiscard]] tay::expected<void, FdtError> enumerate_cpus(
            CatalogBuilder &builder, const void *tree, const boot::Context &context) noexcept {
            const int cpus = fdt_path_offset(tree, "/cpus");
            if (cpus < 0)
                return tay::Err(FdtError::NodeNotFound(node_offset(cpus)));

            u32_t address_cells = 0;
            if (!node_has_property(tree, cpus, "#address-cells"))
                return tay::Err(
                    FdtError::MissingProperty(node_offset(cpus), PropertyId::ADDRESS_CELLS));
            if (!read_u32_property(tree, cpus, "#address-cells", address_cells))
                return tay::Err(
                    FdtError::InvalidProperty(node_offset(cpus), PropertyId::ADDRESS_CELLS));
            if (address_cells == 0 || address_cells > 2)
                return tay::Err(FdtError::CellCountUnsupported(
                    node_offset(cpus), static_cast<u8_t>(address_cells), 0));

            u32_t frequency = 0;
            if (read_u32_property(tree, cpus, "timebase-frequency", frequency)) {
                PlatformFacts facts{};
                facts.memory_regions        = bootinfo_regions(context.info);
                facts.memory_region_count   = context.info->region_cnt;
                facts.timebase_frequency_hz = frequency;
                builder.set_platform(facts);
            } else if (node_has_property(tree, cpus, "timebase-frequency"))
                return tay::Err(
                    FdtError::InvalidProperty(node_offset(cpus), PropertyId::TIMEBASE_FREQUENCY));

            bool found_bsp   = false;
            u32_t logical_id = 0;
            int node         = -1;
            fdt_for_each_subnode(node, tree, cpus) {
                int type_length  = 0;
                const void *type = fdt_getprop(tree, node, "device_type", &type_length);
                if (!property_string_equals(type, type_length, "cpu"))
                    continue;

                int reg_length = 0;
                const auto *reg =
                    static_cast<const fdt32_t *>(fdt_getprop(tree, node, "reg", &reg_length));
                u64_t hardware_id = 0;
                if (!node_enabled(tree, node) || reg == nullptr ||
                    reg_length < static_cast<int>(address_cells * sizeof(fdt32_t)) ||
                    !read_cells(reg, address_cells, hardware_id))
                {
                    continue;
                }

                CpuDescriptor cpu{};
                cpu.logical_id   = logical_id++;
                cpu.hardware_id  = hardware_id;
                cpu.firmware_id  = node_id(tree, node);
                cpu.enabled      = true;
                int model_length = 0;
                const char *model =
                    static_cast<const char *>(fdt_getprop(tree, node, "model", &model_length));
                if (model != nullptr && model_length > 0)
                    copy_text(cpu.model, sizeof(cpu.model), model,
                              static_cast<size_t>(model_length));
                else
                    copy_node_name(tree, node, cpu.model, sizeof(cpu.model));

                const auto result = builder.add_cpu(cpu);
                if (!result)
                    return tay::Err(FdtError::CatalogRejected(result.error().code()));
                if (hardware_id == context.info->hart_id) {
                    builder.set_bsp(cpu.logical_id);
                    found_bsp = true;
                }
            }
            return found_bsp ? tay::expected<void, FdtError>{}
                             : tay::Err(FdtError::BootCpuNotFound(context.info->hart_id));
        }

        [[nodiscard]] tay::expected<void, FdtError> enumerate_devices(CatalogBuilder &builder,
                                                                      const void *tree) noexcept {
            const int cpus = fdt_path_offset(tree, "/cpus");
            int node       = -1;
            while ((node = fdt_next_node(tree, node, nullptr)) >= 0) {
                if (node == cpus || fdt_parent_offset(tree, node) == cpus)
                    continue;

                int compatible_length  = 0;
                const char *compatible = static_cast<const char *>(
                    fdt_getprop(tree, node, "compatible", &compatible_length));
                if (compatible == nullptr || compatible_length <= 0)
                    continue;

                DeviceDescriptor device{};
                device.id        = node_id(tree, node);
                const int parent = fdt_parent_offset(tree, node);
                if (parent >= 0)
                    device.parent = node_id(tree, parent);
                device.device_class   = classify_device(tree, node);
                device.binding_policy = binding_policy(device.device_class);
                device.enabled        = node_enabled(tree, node);
                copy_node_name(tree, node, device.name, sizeof(device.name));
                copy_text(device.compatible, sizeof(device.compatible), compatible,
                          static_cast<size_t>(compatible_length));
                device.first_mmio = first_reg(tree, node);

                const auto result = builder.add_device(device);
                if (!result)
                    return tay::Err(FdtError::CatalogRejected(result.error().code()));
            }
            return {};
        }

        [[nodiscard]] tay::expected<void, FdtError> enumerate_controllers(
            CatalogBuilder &builder, const void *tree) noexcept {
            int node = -1;
            while ((node = fdt_next_node(tree, node, nullptr)) >= 0) {
                if (!node_has_property(tree, node, "interrupt-controller"))
                    continue;

                InterruptControllerDescriptor controller{};
                controller.id        = node_id(tree, node);
                controller.role      = node_compatible_with(tree, node, "riscv,cpu-intc")
                                           ? ControllerRole::CPU_LOCAL
                                           : ControllerRole::ROOT;
                u32_t parent_phandle = 0;
                if (read_u32_property(tree, node, "interrupt-parent", parent_phandle) &&
                    parent_phandle != 0)
                {
                    controller.parent = FirmwareId{.kind         = FirmwareKind::FDT,
                                                   .namespace_id = FDT_NAMESPACE_PHANDLE,
                                                   .local_id     = parent_phandle};
                    controller.role   = ControllerRole::CASCADED;
                }
                (void)read_u32_property(tree, node, "#interrupt-cells", controller.interrupt_cells);
                copy_first_compatible(tree, node, controller.compatible,
                                      sizeof(controller.compatible));
                controller.first_mmio = first_reg(tree, node);

                const auto result = builder.add_controller(controller);
                if (!result)
                    return tay::Err(FdtError::CatalogRejected(result.error().code()));
            }
            return {};
        }
    }  // namespace detail

    tay::expected<void, tay::error_code> Enumerator::enumerate(CatalogBuilder &builder,
                                                               FirmwareInput input) noexcept {
        if (input.kind != FirmwareKind::FDT || input.data == nullptr || input.size == 0 ||
            input.boot_context == nullptr)
        {
            return tay::Err(to_tay_error(FdtError::InvalidBlob()));
        }
        auto result = device::fdt::enumerate(builder, *input.boot_context);
        if (!result) {
            kernel::log::error("FDT 枚举失败: {}", result.error());
            return tay::Err(to_tay_error(result.error()));
        }
        return {};
    }

    tay::expected<void, FdtError> enumerate(CatalogBuilder &builder,
                                            const boot::Context &context) noexcept {
        if (context.info == nullptr || context.fdt == nullptr || context.fdt_sz == 0 ||
            fdt_check_full(context.fdt, context.fdt_sz) != 0)
        {
            return tay::Err(FdtError::InvalidBlob());
        }

        PlatformFacts facts{};
        facts.memory_regions      = bootinfo_regions(context.info);
        facts.memory_region_count = context.info->region_cnt;
        builder.set_platform(facts);

        TAY_TRYV(detail::enumerate_cpus(builder, context.fdt, context));
        TAY_TRYV(detail::enumerate_devices(builder, context.fdt));
        return detail::enumerate_controllers(builder, context.fdt);
    }
}  // namespace device::fdt
