/**
 * @file decode.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 通用解码辅助实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/fdt/decode.h>
#include <logger.h>

#include <algorithm>

namespace fdt {
    std::optional<driver::IrqTrigger> decode_trigger_cell(
        uint32_t raw) noexcept {
        switch (raw) {
            case 1: return driver::IrqTrigger::EDGE_RISING;
            case 2: return driver::IrqTrigger::EDGE_FALLING;
            case 4: return driver::IrqTrigger::LEVEL_HIGH;
            case 8: return driver::IrqTrigger::LEVEL_LOW;
            default: return std::nullopt;
        }
    }

    void normalize_target_harts(
        std::vector<device::cpuid_t> &target_harts) noexcept {
        std::sort(target_harts.begin(), target_harts.end());
        target_harts.erase(std::unique(target_harts.begin(), target_harts.end()),
                           target_harts.end());
    }

    bool node_status_enabled(const Node &node) {
        auto it = node.properties.find(STATUS_PROP);
        if (it == node.properties.end()) {
            return true;
        }

        std::string status = it->second->as_string();
        return status == OKAY_STATUS || status == OK_STATUS;
    }

    RegionCells node_region_cells(const Node &node) {
        size_t addr_cells = 2;
        size_t size_cells = 2;

        if (node.parent != nullptr) {
            auto addr_it = node.parent->properties.find(ADDRESS_CELLS_PROP);
            if (addr_it != node.parent->properties.end()) {
                addr_cells =
                    static_cast<size_t>(addr_it->second->as_integral());
            }

            auto size_it = node.parent->properties.find(SIZE_CELLS_PROP);
            if (size_it != node.parent->properties.end()) {
                size_cells =
                    static_cast<size_t>(size_it->second->as_integral());
            }
        }

        return {.addr_cells = addr_cells, .size_cells = size_cells};
    }

    bool has_property(const Node &node, const char *prop_name) {
        return node.properties.contains(prop_name);
    }

    bool is_string_prop_equal(const Node &node, const char *prop_name,
                              const char *expected) {
        auto it = node.properties.find(prop_name);
        if (it == node.properties.end()) {
            return false;
        }
        return it->second->as_string() == expected;
    }

    std::optional<uint64_t> parse_reg_value(const Node &node) noexcept {
        auto reg_it = node.properties.find(REG_PROP);
        if (reg_it == node.properties.end()) {
            return std::nullopt;
        }
        return reg_it->second->as_integral();
    }

    std::vector<const Node *> sorted_children(const Node &node) {
        std::vector<const Node *> result;
        result.reserve(node.children.size());
        for (const auto &[_, child] : node.children) {
            result.push_back(child.get());
        }
        std::sort(result.begin(), result.end(),
                  [](const Node *lhs, const Node *rhs) {
                      return lhs->name < rhs->name;
                  });
        return result;
    }

    std::string fallback_cpu_model(const Node &node) {
        auto model_it = node.properties.find(MODEL_PROP);
        if (model_it != node.properties.end()) {
            std::string model = model_it->second->as_string();
            if (!model.empty()) {
                return model;
            }
        }

        auto compatible_it = node.properties.find(COMPATIBLE_PROP);
        if (compatible_it != node.properties.end()) {
            auto compatibles = compatible_it->second->as_string_list();
            if (!compatibles.empty()) {
                return compatibles.front();
            }
        }

        return node.name;
    }

    std::optional<phandle_t> find_local_intc_phandle(
        const Node &cpu_node) noexcept {
        for (const auto &[_, child] : cpu_node.children) {
            if (!has_property(*child, INTC_PROP)) {
                continue;
            }
            if (child->phandle != 0) {
                return child->phandle;
            }
            loggers::DEVICE::WARN(
                "CPU 节点 %s 的 interrupt-controller 缺少 phandle",
                cpu_node.name.c_str());
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::vector<uint32_t> parse_u32_cells(const Property &prop) {
        constexpr size_t CELL_SIZE = sizeof(uint32_t);
        if (prop.size % CELL_SIZE != 0) {
            return {};
        }

        std::vector<uint32_t> cells;
        cells.reserve(prop.size / CELL_SIZE);
        for (size_t offset = 0; offset < prop.size; offset += CELL_SIZE) {
            uint64_t value = 0;
            if (!fdt::detail::parse_be_integer(prop.data + offset, CELL_SIZE,
                                               value))
            {
                return {};
            }
            cells.push_back(static_cast<uint32_t>(value));
        }
        return cells;
    }
}  // namespace fdt
