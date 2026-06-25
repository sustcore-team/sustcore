/**
 * @file decode.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 通用解码辅助
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/fdt/tree.h>
#include <driver/int/base.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace fdt {
    constexpr const char *PHANDLE_PROP          = "phandle";
    constexpr const char *LINUX_PHANDLE_PROP    = "linux,phandle";
    constexpr const char *REG_PROP              = "reg";
    constexpr const char *ADDRESS_CELLS_PROP    = "#address-cells";
    constexpr const char *SIZE_CELLS_PROP       = "#size-cells";
    constexpr const char *NO_MAP_PROP           = "no-map";
    constexpr const char *TIMEBASE_FREQ_PROP    = "timebase-frequency";
    constexpr const char *MODEL_PROP            = "model";
    constexpr const char *MMU_TYPE_PROP         = "mmu-type";
    constexpr const char *RISCV_ISA_PROP        = "riscv,isa";
    constexpr const char *CPU_PROP              = "cpu";
    constexpr const char *INTERRUPTS_PROP       = "interrupts";
    constexpr const char *INTERRUPT_EXT_PROP    = "interrupts-extended";
    constexpr const char *INTERRUPT_PARENT_PROP = "interrupt-parent";
    constexpr const char *INTC_PROP             = "interrupt-controller";
    constexpr const char *INTERRUPT_CELLS_PROP  = "#interrupt-cells";
    constexpr const char *CPU_MAP_NODE          = "cpu-map";
    constexpr const char *RISCV_NDEV_PROP       = "riscv,ndev";
    constexpr const char *STDOUT_PATH_PROP      = "stdout-path";
    constexpr const char *LOONGARCH_CPUIC_COMPATIBLE =
        "loongson,cpu-interrupt-controller";

    constexpr const char *MEMORY_DEVICE_TYPE = "memory";
    constexpr const char *CPU_DEVICE_TYPE    = "cpu";
    constexpr const char *CLINT_COMPATIBLE   = "riscv,clint0";
    constexpr const char *PLIC_COMPATIBLE    = "riscv,plic0";

    constexpr const char *RESERVED_MEMORY_PATH = "/reserved-memory";
    constexpr const char *CPUS_PATH            = "/cpus";
    constexpr const char *CHOSEN_PATH          = "/chosen";
    constexpr const char *ALIASES_PATH         = "/aliases";

    [[nodiscard]]
    std::optional<driver::IrqTrigger> decode_trigger_cell(
        uint32_t raw) noexcept;

    void normalize_target_harts(
        std::vector<device::cpuid_t> &target_harts) noexcept;

    [[nodiscard]]
    bool node_status_enabled(const Node &node);

    [[nodiscard]]
    RegionCells node_region_cells(const Node &node);

    [[nodiscard]]
    bool has_property(const Node &node, const char *prop_name);

    [[nodiscard]]
    bool is_string_prop_equal(const Node &node, const char *prop_name,
                              const char *expected);

    [[nodiscard]]
    std::optional<uint64_t> parse_reg_value(const Node &node) noexcept;

    [[nodiscard]]
    std::vector<const Node *> sorted_children(const Node &node);

    [[nodiscard]]
    std::string fallback_cpu_model(const Node &node);

    [[nodiscard]]
    std::optional<phandle_t> find_local_intc_phandle(
        const Node &cpu_node) noexcept;

    [[nodiscard]]
    std::vector<uint32_t> parse_u32_cells(const Property &prop);
}  // namespace fdt
