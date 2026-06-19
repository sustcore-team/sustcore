/**
 * @file internal.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 内部共享辅助
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/fdt/device_node.h>
#include <driver/model.h>

#include <limits>
#include <optional>
#include <ranges>
#include <unordered_set>

namespace fdt {
    struct ParsedCpu {
        device::cpuid_t id;
        std::string model;
        std::string isa_string;
        std::string mmu_type;
        phandle_t cpu_phandle;
        phandle_t local_intc_phandle;
        phandle_t cpu_intc_phandle;
    };

    struct ParsedInterruptRef {
        phandle_t phandle = 0;
        driver::hwirq_t hwirq = 0;
        std::optional<driver::IrqTrigger> trigger = std::nullopt;
    };

    [[nodiscard]]
    const driver::IIrqChipFactory *find_irq_factory_for_node(
        const driver::DriverModel &driver_model,
        const device::DeviceNode &node) noexcept;

    [[nodiscard]]
    Result<std::vector<phandle_t>> interrupt_parent_phandles_for_node(
        const FDTProvider &provider, const Node &node);

    [[nodiscard]]
    std::vector<device::cpuid_t> target_harts_from_interrupt_refs(
        const char *node_name,
        const std::vector<ParsedInterruptRef> &refs,
        const LocalInterruptTargetMap &local_intc_map);

    [[nodiscard]]
    std::optional<device::CpuTopoLevel> topo_level_from_name(
        std::string_view name) noexcept;

    [[nodiscard]]
    Result<std::vector<device::cpuid_t>> build_cpu_map_subtree(
        device::CpuTopologyBuilder &builder, const Node &map_node,
        device::topo_t parent_id, device::topo_t &next_topo_id,
        const std::unordered_map<phandle_t, device::cpuid_t> &cpu_phandle_map);

    [[nodiscard]]
    Result<device::CpuTopology> build_default_topology(
        const std::vector<device::cpuid_t> &cpu_ids);

    [[nodiscard]]
    Result<device::CpuTopology> build_cpu_map_topology(
        const Node &cpu_map_node,
        const std::unordered_map<phandle_t, device::cpuid_t> &cpu_phandle_map,
        const std::vector<device::cpuid_t> &all_cpu_ids);

    [[nodiscard]]
    Result<ParsedCpu> parse_cpu_node(const Node &cpu_node);
}  // namespace fdt
