/**
 * @file provider.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT Provider
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/factory.h>
#include <device/fdt/tree.h>
#include <device/model.h>
#include <logger.h>
#include <sus/types.h>

#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fdt {
    class FDTDeviceNode;

    class FDTProvider : public device::DeviceProvider {
        friend class FDTDeviceNode;

    private:
        struct InterruptRef {
            phandle_t phandle = 0;
            hwirq_t hwirq = 0;
            std::optional<driver::IrqTrigger> trigger = std::nullopt;
        };

        Configuration _config;
        mutable std::unordered_map<phandle_t, domain_t> _irq_domains;
        mutable LocalInterruptTargetMap _local_intc_map;
        mutable std::vector<CpuIntcDescriptor> _cpu_intc_candidates;

        [[nodiscard]]
        Result<void> register_irq_domain(phandle_t phandle,
                                         const driver::IrqDomain &domain) const;
        [[nodiscard]]
        Result<driver::IrqDomain &> resolve_irq_domain(
            phandle_t phandle, driver::IrqManager &irqman) const;
        [[nodiscard]]
        Result<size_t> interrupt_cells_for_controller(
            phandle_t controller_phandle) const;
        [[nodiscard]]
        Result<domain_t> resolve_interrupt_domain(phandle_t phandle) const;
        [[nodiscard]]
        Result<std::vector<driver::virq_t>> resolve_interrupt_refs_to_virqs(
            const std::vector<InterruptRef> &refs,
            driver::IrqManager &irqman) const;
        [[nodiscard]]
        Result<std::vector<driver::virq_t>> parse_interrupt_virqs(
            const Node &node, driver::IrqManager &irqman) const;
        [[nodiscard]]
        Result<std::vector<device::RawIrqSpec>> resolve_interrupt_refs_to_specs(
            const std::vector<InterruptRef> &refs) const;
        [[nodiscard]]
        Result<std::vector<device::RawIrqSpec>> parse_interrupt_specs(
            const Node &node) const;

        void append_as_regions(std::vector<MemRegion> &regions,
                               const RegionCells &cells, const Property &prop,
                               MemRegion::MemoryStatus status) const;

        bool is_memory_node(Node &node) const;
        [[nodiscard]]
        bool node_is_simple_bus(const FDTDeviceNode &node) const noexcept;
        [[nodiscard]]
        Result<device::DeviceNode *> make_device_node(
            const Node &node, device::DeviceModel &model) const;
        template <typename Fn>
        void scan_visible_nodes(const Node &root, Fn &&handler) const;

        void register_memory_regions(device::DeviceModel &model) const;
        void register_platform(device::DeviceModel &model) const;
        void register_cpus(device::DeviceModel &model) const;
        void register_nodes(device::DeviceModel &model) const;
        void register_intcs(device::DeviceModel &model) const;
        void register_clock_virq(device::DeviceModel &model) const noexcept;

    public:
        explicit FDTProvider(void *dtb);
        [[nodiscard]]
        std::optional<phandle_t> maybe_interrupt_parent(
            const Node &node) const noexcept;
        [[nodiscard]]
        Result<phandle_t> resolve_interrupt_parent(const Node &node) const;
        [[nodiscard]]
        Result<std::vector<InterruptRef>> parse_interrupts_extended(
            const Node &node) const;
        [[nodiscard]]
        Result<std::vector<InterruptRef>> parse_interrupts(
            const Node &node) const;
        void init_device_factories() noexcept;
        void init_irq_factories() noexcept;

        [[nodiscard]]
        Result<std::vector<InterruptRef>> parse_interrupts_extended_view(
            const Node &node) const {
            return parse_interrupts_extended(node);
        }

        [[nodiscard]]
        Result<std::vector<driver::virq_t>> parse_interrupt_virqs_view(
            const Node &node, driver::IrqManager &irqman) const {
            return parse_interrupt_virqs(node, irqman);
        }

        [[nodiscard]]
        Result<driver::IrqDomain &> resolve_irq_domain_view(
            phandle_t phandle, driver::IrqManager &irqman) const {
            return resolve_irq_domain(phandle, irqman);
        }

        [[nodiscard]]
        Result<void> register_irq_domain_view(
            phandle_t phandle, const driver::IrqDomain &domain) const {
            return register_irq_domain(phandle, domain);
        }

        [[nodiscard]]
        const Configuration &config() const noexcept {
            return _config;
        }

        [[nodiscard]]
        std::vector<CpuIntcDescriptor> &cpu_intc_candidates() const noexcept {
            return _cpu_intc_candidates;
        }

        [[nodiscard]]
        LocalInterruptTargetMap &local_intc_map() const noexcept {
            return _local_intc_map;
        }

        FDTProvider(void *dtb, int) = delete;
        FDTProvider(const FDTProvider &)            = delete;
        FDTProvider &operator=(const FDTProvider &) = delete;
        FDTProvider(FDTProvider &&)                 = delete;
        FDTProvider &operator=(FDTProvider &&)      = delete;

        [[nodiscard]]
        Result<void> register_device(device::DeviceModel &model) const override;
        [[nodiscard]]
        const char *name() const override {
            return "fdt";
        }
    };
}  // namespace fdt
