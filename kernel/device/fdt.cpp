/**
 * @file fdt.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 设备提供器实现
 * @version alpha-1.0.0
 * @date 2026-05-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/clint.h>
#include <arch/riscv64/clock.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/device/platform.h>
#include <arch/riscv64/intc.h>
#include <arch/riscv64/plic.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/fdt_helper.h>
#endif
#include <device/fdt.h>
#include <device/model.h>
#include <driver/model.h>
#include <driver/rtc/goldfish.h>
#include <driver/serial.h>
#include <driver/virtio/virtio-blk.h>
#include <driver/virtio/virtio.h>
#include <libfdt.h>
#include <logger.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace {
    using driver::domain_t;
    using driver::DriverBase;
    using driver::hwirq_t;
    using driver::intc_t;
    using driver::virq_t;

    constexpr const char *PHANDLE_PROP          = "phandle";
    constexpr const char *LINUX_PHANDLE_PROP    = "linux,phandle";
    constexpr const char *REG_PROP              = "reg";
    constexpr const char *ADDRESS_CELLS_PROP    = "#address-cells";
    constexpr const char *SIZE_CELLS_PROP       = "#size-cells";
    constexpr const char *NO_MAP_PROP           = "no-map";
    constexpr const char *STATUS_PROP           = "status";
    constexpr const char *DEVICE_TYPE_PROP      = "device_type";
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

    constexpr const char *OKAY_STATUS        = "okay";
    constexpr const char *MEMORY_DEVICE_TYPE = "memory";
    constexpr const char *CPU_DEVICE_TYPE    = "cpu";
    constexpr const char *CLINT_COMPATIBLE   = "riscv,clint0";
    constexpr const char *PLIC_COMPATIBLE    = "riscv,plic0";
    constexpr size_t CLINT_MAX_HW_IRQ        = 16;
    constexpr size_t MAX_PLIC_IRQS           = 256;

    constexpr const char *RESERVED_MEMORY_PATH = "/reserved-memory";
    constexpr const char *CPUS_PATH            = "/cpus";

    struct ParsedCpu {
        device::cpuid_t id;
        std::string model;
        std::string isa_string;
        std::string mmu_type;
        fdt::phandle_t cpu_phandle;
        fdt::phandle_t local_intc_phandle;
        fdt::phandle_t cpu_intc_phandle;
    };

    /**
     * @brief 本地中断端点 phandle 到目标 hart 的映射.
     */
    using LocalInterruptTargetMap =
        std::unordered_map<fdt::phandle_t, device::cpuid_t>;

    /**
     * @brief 将 hart 列表排序并去重.
     *
     * @param target_harts 待规范化的 hart 列表.
     */
    void normalize_target_harts(
        std::vector<device::cpuid_t> &target_harts) noexcept {
        std::ranges::sort(target_harts);
        target_harts.erase(
            std::ranges::unique(target_harts,
                                [](device::cpuid_t lhs, device::cpuid_t rhs) {
                                    return lhs == rhs;
                                }),
            target_harts.end());
    }

    /**
     * @brief 判断节点状态是否为启用.
     */
    [[nodiscard]]
    bool node_status_enabled(const fdt::Node &node) {
        auto it = node.properties.find(STATUS_PROP);
        if (it == node.properties.end()) {
            return true;
        }

        std::string status = it->second->as_string();
        return status == fdt::OKAY_STATUS || status == fdt::OK_STATUS;
    }

    /**
     * @brief 获取节点 reg 属性的地址/长度 cell 配置.
     */
    [[nodiscard]]
    fdt::RegionCells node_region_cells(const fdt::Node &node) {
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

    /**
     * @brief 递归构建设备树节点对象.
     */
    void build_node_recursive(const void *dtb, fdt::Configuration &config,
                              fdt::Node &node, int node_offset) {
        int prop_offset;
        fdt_for_each_property_offset(prop_offset, dtb, node_offset) {
            const char *prop_name = nullptr;
            int prop_len          = 0;
            const void *prop_data =
                fdt_getprop_by_offset(dtb, prop_offset, &prop_name, &prop_len);
            if (prop_name == nullptr || prop_data == nullptr || prop_len < 0) {
                continue;
            }

            auto *prop = new fdt::Property{
                .name = prop_name,
                .data = static_cast<const char *>(prop_data),
                .size = static_cast<size_t>(prop_len),
            };
            node.properties[prop->name] = util::owner(prop);

            if ((prop->name == PHANDLE_PROP ||
                 prop->name == LINUX_PHANDLE_PROP) &&
                node.phandle == 0)
            {
                node.phandle = prop->as_phandle();
            }
        }

        if (node.phandle != 0) {
            config.phandle_map[node.phandle] = &node;
        }

        int child_offset;
        fdt_for_each_subnode(child_offset, dtb, node_offset) {
            const char *child_name = fdt_get_name(dtb, child_offset, nullptr);
            if (child_name == nullptr) {
                continue;
            }

            auto child    = util::owner(new fdt::Node());
            child->name   = child_name;
            child->parent = &node;

            build_node_recursive(dtb, config, *child, child_offset);
            node.children[child->name] = std::move(child);
        }
    }

    /**
     * @brief 判断节点是否存在指定属性.
     */
    [[nodiscard]]
    bool has_property(const fdt::Node &node, const char *prop_name) {
        return node.properties.contains(prop_name);
    }

    /**
     * @brief 判断字符串属性是否等于给定值.
     */
    [[nodiscard]]
    bool is_string_prop_equal(const fdt::Node &node, const char *prop_name,
                              const char *expected) {
        auto it = node.properties.find(prop_name);
        if (it == node.properties.end()) {
            return false;
        }
        return it->second->as_string() == expected;
    }

    /**
     * @brief 解析节点的 reg 单整数值.
     */
    [[nodiscard]]
    std::optional<uint64_t> parse_reg_value(const fdt::Node &node) noexcept {
        auto reg_it = node.properties.find(REG_PROP);
        if (reg_it == node.properties.end()) {
            return std::nullopt;
        }
        return reg_it->second->as_integral();
    }

    /**
     * @brief 按名称排序返回子节点列表.
     */
    [[nodiscard]]
    std::vector<const fdt::Node *> sorted_children(const fdt::Node &node) {
        std::vector<const fdt::Node *> result;
        result.reserve(node.children.size());
        for (const auto &[_, child] : node.children) {
            result.push_back(child.get());
        }
        std::ranges::sort(result,
                          [](const fdt::Node *lhs, const fdt::Node *rhs) {
                              return lhs->name < rhs->name;
                          });
        return result;
    }

    /**
     * @brief 为 CPU 节点推导可读型号字符串.
     */
    [[nodiscard]]
    std::string fallback_cpu_model(const fdt::Node &node) {
        auto model_it = node.properties.find(MODEL_PROP);
        if (model_it != node.properties.end()) {
            std::string model = model_it->second->as_string();
            if (!model.empty()) {
                return model;
            }
        }

        auto compatible_it = node.properties.find(fdt::COMPATIBLE_PROP);
        if (compatible_it != node.properties.end()) {
            auto compatibles = compatible_it->second->as_string_list();
            if (!compatibles.empty()) {
                return compatibles.front();
            }
        }

        return node.name;
    }

    /**
     * @brief 提取 CPU 节点下本地中断控制器的 phandle.
     */
    [[nodiscard]]
    std::optional<fdt::phandle_t> find_local_intc_phandle(
        const fdt::Node &cpu_node) noexcept {
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

    /**
     * @brief 将属性数据解析为 u32 cell 数组.
     */
    [[nodiscard]]
    std::vector<uint32_t> parse_u32_cells(const fdt::Property &prop) {
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

    /**
     * @brief 从中断引用列表中提取目标 hart 集合.
     *
     * @param node_name 当前设备节点名称, 仅用于日志.
     * @param refs 中断引用列表.
     * @param local_intc_map 本地中断端点到 hart 的映射.
     * @return std::vector<device::cpuid_t> 去重后的目标 hart 列表.
     */
    [[nodiscard]]
    std::vector<device::cpuid_t> target_harts_from_interrupt_refs(
        const char *node_name,
        const std::vector<std::pair<fdt::phandle_t, hwirq_t>> &refs,
        const LocalInterruptTargetMap &local_intc_map) {
        std::vector<device::cpuid_t> target_harts;
        target_harts.reserve(refs.size());

        for (const auto &[phandle, hwirq] : refs) {
            auto cpu_it = local_intc_map.find(phandle);
            if (cpu_it == local_intc_map.end()) {
                loggers::DEVICE::DEBUG(
                    "%s 的中断引用未匹配到本地 hart: phandle=%u hwirq=%u",
                    node_name, phandle, static_cast<unsigned>(hwirq));
                continue;
            }
            target_harts.push_back(cpu_it->second);
        }

        normalize_target_harts(target_harts);
        return target_harts;
    }

    /**
     * @brief 将 cpu-map 节点名映射为拓扑层级.
     */
    [[nodiscard]]
    std::optional<device::CpuTopoLevel> topo_level_from_name(
        std::string_view name) noexcept {
        if (name.starts_with("thread")) {
            return device::CpuTopoLevel::THREAD;
        }
        if (name.starts_with("core")) {
            return device::CpuTopoLevel::CORE;
        }
        if (name.starts_with("cluster")) {
            return device::CpuTopoLevel::CLUSTER;
        }
        if (name.starts_with("package") || name.starts_with("socket")) {
            return device::CpuTopoLevel::PACKAGE;
        }
        if (name.starts_with("numa")) {
            return device::CpuTopoLevel::NUMA;
        }
        return std::nullopt;
    }

    /**
     * @brief 递归构建 cpu-map 子树.
     */
    [[nodiscard]]
    Result<std::vector<device::cpuid_t>> build_cpu_map_subtree(
        device::CpuTopologyBuilder &builder, const fdt::Node &map_node,
        device::topo_t parent_id, device::topo_t &next_topo_id,
        const std::unordered_map<fdt::phandle_t, device::cpuid_t>
            &cpu_phandle_map) {
        // 识别当前拓扑层级.
        auto level = topo_level_from_name(map_node.name);
        if (!level.has_value()) {
            loggers::DEVICE::WARN("忽略未知 cpu-map 拓扑节点: %s",
                                  map_node.name.c_str());
            return std::vector<device::cpuid_t>{};
        }

        device::topo_t node_id = next_topo_id++;
        auto add_res           = builder.add_child(parent_id, *level, node_id);
        if (!add_res.has_value()) {
            propagate_return(add_res);
        }

        // 收集当前节点直接声明的 CPU.
        std::vector<device::cpuid_t> aggregated;
        auto cpu_it = map_node.properties.find(CPU_PROP);
        if (cpu_it != map_node.properties.end()) {
            fdt::phandle_t cpu_phandle = cpu_it->second->as_phandle();
            auto cpu_map_it            = cpu_phandle_map.find(cpu_phandle);
            if (cpu_map_it == cpu_phandle_map.end()) {
                loggers::DEVICE::ERROR(
                    "cpu-map 节点 %s 引用了未知 CPU phandle=%u",
                    map_node.name.c_str(), cpu_phandle);
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            aggregated.push_back(cpu_map_it->second);
        }

        // 递归构建子节点.
        for (const auto *child : sorted_children(map_node)) {
            auto child_res = build_cpu_map_subtree(
                builder, *child, node_id, next_topo_id, cpu_phandle_map);
            propagate(child_res);
            const auto &child_cpus = child_res.value();
            aggregated.insert(aggregated.end(), child_cpus.begin(),
                              child_cpus.end());
        }

        // 规范化 CPU 列表并回填到节点.
        std::ranges::sort(aggregated);
        aggregated.erase(
            std::ranges::unique(aggregated,
                                [](device::cpuid_t lhs, device::cpuid_t rhs) {
                                    return lhs == rhs;
                                }),
            aggregated.end());

        auto cpus_res = builder.cpus(node_id, aggregated);
        propagate(cpus_res);
        return aggregated;
    }

    /**
     * @brief 构造保底 CPU 拓扑.
     */
    [[nodiscard]]
    Result<device::CpuTopology> build_default_topology(
        const std::vector<device::cpuid_t> &cpu_ids) {
        device::CpuTopologyBuilder builder;

        // 构造根 cluster.
        builder.root(device::CpuTopoLevel::CLUSTER, 0);

        auto root_res = builder.cpus(0, cpu_ids);
        propagate(root_res);

        // 为每个 CPU 创建独立 core 节点.
        device::topo_t next_id = 1;
        for (device::cpuid_t cpu_id : cpu_ids) {
            auto add_res =
                builder.add_child(0, device::CpuTopoLevel::CORE, next_id);
            propagate(add_res);
            auto cpu_res = builder.cpus(next_id, {cpu_id});
            propagate(cpu_res);
            ++next_id;
        }

        return builder.build();
    }

    /**
     * @brief 从 cpu-map 构建 CPU 拓扑.
     */
    [[nodiscard]]
    Result<device::CpuTopology> build_cpu_map_topology(
        const fdt::Node &cpu_map_node,
        const std::unordered_map<fdt::phandle_t, device::cpuid_t>
            &cpu_phandle_map,
        const std::vector<device::cpuid_t> &all_cpu_ids) {
        // 收集顶层拓扑节点.
        auto top_children = sorted_children(cpu_map_node);
        if (top_children.empty()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        device::CpuTopologyBuilder builder;
        device::topo_t next_topo_id = 1;

        // 单根拓扑直接将该节点提升为树根.
        if (top_children.size() == 1) {
            auto root_level = topo_level_from_name(top_children.front()->name);
            if (!root_level.has_value()) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            builder.root(*root_level, 0);

            std::vector<device::cpuid_t> aggregated;
            auto cpu_it = top_children.front()->properties.find(CPU_PROP);
            if (cpu_it != top_children.front()->properties.end()) {
                fdt::phandle_t cpu_phandle = cpu_it->second->as_phandle();
                auto cpu_map_it            = cpu_phandle_map.find(cpu_phandle);
                if (cpu_map_it == cpu_phandle_map.end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                aggregated.push_back(cpu_map_it->second);
            }

            for (const auto *child : sorted_children(*top_children.front())) {
                auto child_res = build_cpu_map_subtree(
                    builder, *child, 0, next_topo_id, cpu_phandle_map);
                propagate(child_res);
                const auto &child_cpus = child_res.value();
                aggregated.insert(aggregated.end(), child_cpus.begin(),
                                  child_cpus.end());
            }

            std::ranges::sort(aggregated);
            aggregated.erase(
                std::ranges::unique(
                    aggregated, [](device::cpuid_t lhs,
                                   device::cpuid_t rhs) { return lhs == rhs; }),
                aggregated.end());
            auto root_res = builder.cpus(0, aggregated);
            propagate(root_res);
        } else {
            // 多个顶层节点时，额外插入一个 package 根节点.
            builder.root(device::CpuTopoLevel::PACKAGE, 0);
            auto root_res = builder.cpus(0, all_cpu_ids);
            propagate(root_res);

            for (const auto *child : top_children) {
                auto child_res = build_cpu_map_subtree(
                    builder, *child, 0, next_topo_id, cpu_phandle_map);
                propagate(child_res);
            }
        }

        return builder.build();
    }

    /**
     * @brief 解析单个 CPU 节点.
     */
    [[nodiscard]]
    Result<ParsedCpu> parse_cpu_node(const fdt::Node &cpu_node) {
        // 基本状态与类型检查.
        if (!node_status_enabled(cpu_node)) {
            unexpect_return(ErrCode::BUSY);
        }
        if (!is_string_prop_equal(cpu_node, DEVICE_TYPE_PROP, CPU_DEVICE_TYPE))
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        // 解析 CPU 编号与 phandle.
        auto reg_value = parse_reg_value(cpu_node);
        if (!reg_value.has_value()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 reg 属性",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (*reg_value > std::numeric_limits<device::cpuid_t>::max()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 的 reg 超出 cpuid 范围: %llu",
                                   cpu_node.name.c_str(),
                                   static_cast<unsigned long long>(*reg_value));
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        if (cpu_node.phandle == 0) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 phandle",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        // 解析 ISA 与本地中断控制器.
        auto isa_it = cpu_node.properties.find(RISCV_ISA_PROP);
        if (isa_it == cpu_node.properties.end()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 riscv,isa 属性",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto local_intc = find_local_intc_phandle(cpu_node);
        if (!local_intc.has_value()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少本地 interrupt-controller",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        ParsedCpu parsed{
            .id                 = static_cast<device::cpuid_t>(*reg_value),
            .model              = fallback_cpu_model(cpu_node),
            .isa_string         = isa_it->second->as_string(),
            .mmu_type           = cpu_node.properties.contains(MMU_TYPE_PROP)
                                      ? cpu_node.properties.at(MMU_TYPE_PROP)->as_string()
                                      : "",
            .cpu_phandle        = cpu_node.phandle,
            .local_intc_phandle = *local_intc,
            .cpu_intc_phandle   = *local_intc,
        };
        return parsed;
    }
}  // namespace

namespace fdt {
    /**
     * @brief 使用 FDT 上下文构造统一设备节点包装器.
     */
    FDTDeviceNode::FDTDeviceNode(const FDTProvider &provider,
                                 const Configuration &config,
                                 const Node &node) noexcept
        : _provider(&provider), _config(&config), _node(&node) {}

    /**
     * @brief 获取统一设备节点名称.
     */
    const char *FDTDeviceNode::name() const noexcept {
        if (_node == nullptr) {
            return "unknown";
        }
        return _node->name.c_str();
    }

    /**
     * @brief 获取节点所属平台名称.
     */
    const char *FDTDeviceNode::platform() const noexcept {
        return "fdt";
    }

    /**
     * @brief 查询统一语义下的节点属性.
     */
    Optional<device::DevicePropView> FDTDeviceNode::property(
        const std::string_view &name) const {
        if (_node == nullptr || _provider == nullptr || _config == nullptr) {
            return std::nullopt;
        }

        if (name == device::STANDARD_COMPATIBLE_KEY) {
            return raw_property(COMPATIBLE_PROP);
        }
        if (name == device::STANDARD_MMIO_KEY) {
            return mmio_property();
        }
        if (name == device::STANDARD_IRQ_KEY) {
            return irq_property();
        }
        if (name == device::STANDARD_INTERRUPT_PARENT_KEY) {
            return interrupt_parent_property();
        }
        return raw_property(std::string(name).c_str());
    }

    /**
     * @brief 读取原始 FDT 属性并包装为统一属性视图.
     */
    Optional<device::DevicePropView> FDTDeviceNode::raw_property(
        const char *prop_name) const noexcept {
        if (_node == nullptr || prop_name == nullptr) {
            return std::nullopt;
        }

        auto prop_it = _node->properties.find(prop_name);
        if (prop_it == _node->properties.end()) {
            return std::nullopt;
        }

        const auto &prop = *prop_it->second;
        device::DevicePropView::PropType type =
            device::DevicePropView::PropType::ANY;
        if (prop.size == 0) {
            type = device::DevicePropView::PropType::NONE;
        } else if (std::strcmp(prop_name, COMPATIBLE_PROP) == 0 ||
                   std::strcmp(prop_name, STATUS_PROP) == 0 ||
                   std::strcmp(prop_name, DEVICE_TYPE_PROP) == 0)
        {
            auto strings = prop.as_string_list();
            type         = strings.size() <= 1
                               ? device::DevicePropView::PropType::STRING
                               : device::DevicePropView::PropType::STRING_LIST;
        } else if (prop.size == sizeof(sus_u32)) {
            type = device::DevicePropView::PropType::INTEGER;
        } else if (prop.size % sizeof(sus_u32) == 0) {
            type = device::DevicePropView::PropType::INTEGER_LIST;
        } else {
            type = device::DevicePropView::PropType::BYTE_ARRAY;
        }

        return device::DevicePropView(
            type, reinterpret_cast<const byte *>(prop.data), prop.size);
    }

    /**
     * @brief 解析当前节点的 MMIO 区域列表.
     */
    Optional<device::DevicePropView> FDTDeviceNode::mmio_property()
        const noexcept {
        if (_node == nullptr) {
            return std::nullopt;
        }

        auto reg_it = _node->properties.find(REG_PROP);
        if (reg_it == _node->properties.end()) {
            loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 缺少 reg 属性",
                                   _node->name.c_str());
            return std::nullopt;
        }

        auto regions = reg_it->second->as_regions(node_region_cells(*_node));
        loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 解析 mmio 区域数=%u",
                               _node->name.c_str(),
                               static_cast<unsigned>(regions.size()));
        return device::DevicePropView::from_region_list(std::move(regions));
    }

    /**
     * @brief 解析当前节点的统一 IRQ 列表.
     */
    Optional<device::DevicePropView> FDTDeviceNode::irq_property()
        const noexcept {
        if (_node == nullptr || !device::DeviceModel::initialized()) {
            return std::nullopt;
        }
        if (!_node->properties.contains(INTERRUPT_EXT_PROP) &&
            !_node->properties.contains(INTERRUPTS_PROP))
        {
            loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 未声明 irqs",
                                   _node->name.c_str());
            return std::nullopt;
        }

        return device::DevicePropView::from_virq_list([provider = _provider,
                                                       node     = _node]() {
            loggers::DEVICE::DEBUG("FDTDeviceNode[%s] 延迟构造 irqs 视图",
                                   node->name.c_str());
            auto &irqman   = device::DeviceModel::inst().interrupt();
            auto virqs_res = provider->parse_interrupt_virqs(*node, irqman);
            if (!virqs_res.has_value()) {
                if (virqs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                    loggers::DEVICE::ERROR(
                        "FDTDeviceNode[%s] 延迟解析 irqs 失败: %s",
                        node->name.c_str(), to_cstring(virqs_res.error()));
                }
                return std::vector<virq_t>{};
            }
            loggers::DEVICE::DEBUG(
                "FDTDeviceNode[%s] 延迟解析 irqs 数量=%u", node->name.c_str(),
                static_cast<unsigned>(virqs_res.value().size()));
            return virqs_res.value();
        });
    }

    /**
     * @brief 解析当前节点的中断父节点标识.
     */
    Optional<device::DevicePropView> FDTDeviceNode::interrupt_parent_property()
        const noexcept {
        if (_node == nullptr) {
            return std::nullopt;
        }

        auto parent_res = _provider->resolve_interrupt_parent(*_node);
        if (!parent_res.has_value()) {
            if (parent_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                loggers::DEVICE::ERROR(
                    "FDTDeviceNode[%s] 解析 interrupt-parent 失败: %s",
                    _node->name.c_str(), to_cstring(parent_res.error()));
            } else {
                loggers::DEVICE::DEBUG(
                    "FDTDeviceNode[%s] 未声明 interrupt-parent",
                    _node->name.c_str());
            }
            return std::nullopt;
        }

        Node *parent_node = _config->get_node_by_phandle(parent_res.value());
        if (parent_node == nullptr) {
            loggers::DEVICE::ERROR(
                "FDTDeviceNode[%s] interrupt-parent phandle=%u 不存在",
                _node->name.c_str(), parent_res.value());
            return std::nullopt;
        }

        auto prop_it = parent_node->properties.find(PHANDLE_PROP);
        if (prop_it == parent_node->properties.end()) {
            prop_it = parent_node->properties.find(LINUX_PHANDLE_PROP);
        }
        if (prop_it == parent_node->properties.end()) {
            loggers::DEVICE::ERROR(
                "FDTDeviceNode[%s] interrupt-parent 节点 %s 缺少 phandle 属性",
                _node->name.c_str(), parent_node->name.c_str());
            return std::nullopt;
        }

        return device::DevicePropView(
            device::DevicePropView::PropType::INTEGER,
            reinterpret_cast<const byte *>(prop_it->second->data),
            prop_it->second->size);
    }

    Node *Configuration::get_node_by_path(const std::string &path) const {
        if (!root || root.get() == nullptr) {
            return nullptr;
        }
        if (path.empty() || path == "/") {
            return root.get();
        }

        Node *current    = root.get();
        size_t component = 0;
        while (component < path.size()) {
            while (component < path.size() && path[component] == '/') {
                ++component;
            }
            if (component >= path.size()) {
                break;
            }

            size_t end = component;
            while (end < path.size() && path[end] != '/') {
                ++end;
            }

            std::string name = path.substr(component, end - component);
            auto child_it    = current->children.find(name);
            if (child_it == current->children.end()) {
                return nullptr;
            }
            current   = child_it->second.get();
            component = end;
        }
        return current;
    }

    Node *Configuration::get_node_by_phandle(phandle_t phandle) const {
        auto it = phandle_map.find(phandle);
        return it == phandle_map.end() ? nullptr : it->second;
    }

    Result<void> Configuration::add_node(Node *parent,
                                         util::owner<Node *> new_node) {
        if (new_node.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        if (!root) {
            if (parent != nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            root         = std::move(new_node);
            root->parent = nullptr;
            if (root->phandle != 0) {
                phandle_map[root->phandle] = root.get();
            }
            void_return();
        }

        if (parent == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        new_node->parent = parent;
        if (parent->children.contains(new_node->name)) {
            unexpect_return(ErrCode::BUSY);
        }

        if (new_node->phandle != 0) {
            phandle_map[new_node->phandle] = new_node.get();
        }
        parent->children[new_node->name] = std::move(new_node);
        void_return();
    }

    Result<void> Configuration::remove_node(Node *node) {
        if (node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (!root || root.get() == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (node == root.get()) {
            if (root->phandle != 0) {
                phandle_map.erase(root->phandle);
            }
            root = util::owner<Node *>(nullptr);
            void_return();
        }

        Node *parent = node->parent;
        if (parent == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto it = parent->children.find(node->name);
        if (it == parent->children.end() || it->second.get() != node) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (node->phandle != 0) {
            phandle_map.erase(node->phandle);
        }
        parent->children.erase(it);
        void_return();
    }

    void make_config(void *dtb, Configuration &config) {
        config.root = util::owner<Node *>(nullptr);
        config.phandle_map.clear();

        if (dtb == nullptr) {
            assert(false && "dtb must not be null");
            return;
        }
        if (FDTHelper::fdt_init(dtb) == nullptr) {
            assert(false && "fdt_init failed");
            return;
        }

        auto root    = util::owner(new Node());
        root->name   = "/";
        root->parent = nullptr;
        build_node_recursive(static_cast<const void *>(FDTHelper::fdt), config,
                             *root, FDTHelper::get_root_node());
        config.root = std::move(root);
    }
}  // namespace fdt

namespace fdt {
    namespace {
#if defined(__ARCH_riscv64__)
        /**
         * @brief FDT 下的 RISC-V CPU 本地中断工厂.
         */
        class RiscVIntCIrqFactory final : public device::IIrqChipFactory {
        public:
            explicit RiscVIntCIrqFactory(
                std::vector<fdt::CpuIntcDescriptor> *candidates) noexcept
                : _candidates(candidates) {}

            [[nodiscard]]
            std::string_view compatible() const noexcept override {
                return "riscv,cpu-intc";
            }

            [[nodiscard]]
            Result<DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model) const override {
                (void)model;
                if (_candidates == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                loggers::DEVICE::INFO("开始创建 RiscVIntC 驱动: name=%s",
                                      node.name());
                auto &fdt_node      = static_cast<const FDTDeviceNode &>(node);
                const auto *matched = std::ranges::find_if(
                    *_candidates,
                    [&fdt_node](const fdt::CpuIntcDescriptor &desc) {
                        return desc.node != nullptr &&
                               desc.node == &fdt_node.raw_node();
                    });
                if (matched == _candidates->end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                auto virqs = device::DevResManager::get_virq_resource(node);
                auto mmios = device::DevResManager::get_mmio_resource(node);
                auto device_owner_res = riscv::IntC::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs), std::move(mmios)),
                    matched->identifier, matched->hart_id);
                propagate(device_owner_res);
                auto &root = *device_owner_res.value();
                auto domain_res = model.interrupt().get_domain(root.identifier());
                propagate(domain_res);
                auto irq_domain_res = _provider->register_irq_domain_view(
                    static_cast<phandle_t>(root.identifier()),
                    domain_res.value().get());
                propagate(irq_domain_res);
                return static_cast<DriverBase *>(
                    device_owner_res.value().get());
            }

            void bind_provider(const FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            std::vector<fdt::CpuIntcDescriptor> *_candidates = nullptr;
            const FDTProvider *_provider                     = nullptr;
        };

        /**
         * @brief FDT 下的 CLINT IRQ 工厂.
         */
        class ClintIrqFactory final : public device::IIrqChipFactory {
        public:
            explicit ClintIrqFactory(
                fdt::LocalInterruptTargetMap *local_intc_map) noexcept
                : _local_intc_map(local_intc_map) {}

            [[nodiscard]]
            std::string_view compatible() const noexcept override {
                return CLINT_COMPATIBLE;
            }

            [[nodiscard]]
            Result<DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model) const override {
                if (_provider == nullptr || _local_intc_map == nullptr ||
                    std::strcmp(node.platform(), "fdt") != 0)
                {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                loggers::DEVICE::INFO("开始创建 Clint 驱动: name=%s",
                                      node.name());
                auto *fdt_node = static_cast<const FDTDeviceNode *>(&node);

                auto refs_res = _provider->parse_interrupts_extended_view(
                    fdt_node->raw_node());
                propagate(refs_res);
                auto target_harts = target_harts_from_interrupt_refs(
                    fdt_node->raw_node().name.c_str(), refs_res.value(),
                    *_local_intc_map);
                if (target_harts.empty()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                auto virqs = device::DevResManager::get_virq_resource(node);
                auto mmios = device::DevResManager::get_mmio_resource(node);

                auto device_owner_res = riscv::Clint::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs), std::move(mmios)),
                    fdt_node->raw_node().phandle != 0
                        ? static_cast<intc_t>(
                              fdt_node->raw_node().phandle)
                        : static_cast<intc_t>(1),
                    target_harts.front(), target_harts);
                propagate(device_owner_res);
                auto &clint = *device_owner_res.value();
                const auto &fdt_device_node =
                    static_cast<const FDTDeviceNode &>(clint.node());
                auto *domain = new driver::LinearIrqDomain<CLINT_MAX_HW_IRQ>(
                    static_cast<domain_t>(clint.identifier()),
                    clint.name(), clint);
                if (domain == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                auto register_res = model.interrupt().register_domain(
                    util::owner<driver::IrqDomain *>(domain));
                propagate(register_res);
                if (fdt_device_node.raw_node().phandle != 0) {
                    auto irq_domain_res = _provider->register_irq_domain_view(
                        fdt_device_node.raw_node().phandle, *domain);
                    propagate(irq_domain_res);
                }
                auto attach_res =
                    clint.attach_to_parent_domain(model.interrupt(), *domain);
                propagate(attach_res);
                model.set_clock_virq(clint.clock_virq());
                return static_cast<DriverBase *>(
                    device_owner_res.value().get());
            }

            void bind_provider(const FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            const FDTProvider *_provider                  = nullptr;
            fdt::LocalInterruptTargetMap *_local_intc_map = nullptr;
        };

        /**
         * @brief FDT 下的 PLIC IRQ 工厂.
         */
        class PlicIrqFactory final : public device::IIrqChipFactory {
        public:
            [[nodiscard]]
            std::string_view compatible() const noexcept override {
                return PLIC_COMPATIBLE;
            }

            [[nodiscard]]
            Result<DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model) const override {
                if (_provider == nullptr ||
                    std::strcmp(node.platform(), "fdt") != 0)
                {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                loggers::DEVICE::INFO("开始创建 Plic 驱动: name=%s",
                                      node.name());
                auto *fdt_node = static_cast<const FDTDeviceNode *>(&node);

                auto refs_res = _provider->parse_interrupts_extended_view(
                    fdt_node->raw_node());
                propagate(refs_res);

                auto &irqman = model.interrupt();
                auto contexts_res = build_plic_contexts(refs_res.value(), irqman);
                propagate(contexts_res);
                auto contexts = std::move(contexts_res.value());
                if (contexts.empty()) {
                    loggers::DEVICE::ERROR(
                        "PLIC 节点 %s 未解析到任何 external context",
                        node.name());
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                auto node_virqs = node.irqs();
                loggers::DEVICE::DEBUG(
                    "PLIC 节点 %s 解析 context=%u node_virqs=%u",
                    node.name(), static_cast<unsigned>(contexts.size()),
                    static_cast<unsigned>(node_virqs.has_value()
                                              ? node_virqs->size()
                                              : 0));
                auto ndev_it =
                    fdt_node->raw_node().properties.find(RISCV_NDEV_PROP);
                if (ndev_it == fdt_node->raw_node().properties.end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                auto virqs = device::DevResManager::get_virq_resource(node);
                auto mmios = device::DevResManager::get_mmio_resource(node);

                auto device_owner_res = riscv::Plic::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs), std::move(mmios)),
                    fdt_node->raw_node().phandle != 0
                        ? static_cast<intc_t>(
                              fdt_node->raw_node().phandle)
                        : static_cast<intc_t>(3),
                    static_cast<hwirq_t>(
                        ndev_it->second->as_integral()),
                    std::move(contexts));
                propagate(device_owner_res);
                auto &plic = *device_owner_res.value();
                const auto &fdt_device_node =
                    static_cast<const FDTDeviceNode &>(plic.node());
                auto domain_res = model.interrupt().get_domain(plic.identifier());
                propagate(domain_res);
                if (fdt_device_node.raw_node().phandle != 0) {
                    auto irq_domain_res = _provider->register_irq_domain_view(
                        fdt_device_node.raw_node().phandle,
                        domain_res.value().get());
                    propagate(irq_domain_res);
                }
                auto attach_res =
                    plic.attach_to_parent_domain(model.interrupt(),
                                                 domain_res.value().get());
                propagate(attach_res);
                return static_cast<DriverBase *>(
                    device_owner_res.value().get());
            }

            void bind_provider(const FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            /**
             * @brief 将 interrupts-extended 条目转换为 PLIC context 列表.
             *
             * @param refs 中断引用列表.
             * @param irqman 全局中断管理器.
             * @return Result<std::vector<riscv::Plic::Context>> context 列表.
             */
            [[nodiscard]]
            Result<std::vector<riscv::Plic::Context>> build_plic_contexts(
                const std::vector<std::pair<phandle_t, hwirq_t>> &refs,
                driver::IrqManager &irqman) const noexcept {
                std::vector<riscv::Plic::Context> contexts;
                contexts.reserve(refs.size());

                size_t enabled_count  = 0;
                size_t disabled_count = 0;
                for (size_t index = 0; index < refs.size(); ++index) {
                    const auto &[phandle, hwirq] = refs[index];

                    auto context_res =
                        build_plic_context(index, phandle, hwirq, irqman);
                    if (!context_res.has_value()) {
                        propagate_return(context_res);
                    }

                    auto &context = context_res.value();
                    if (context.enabled) {
                        ++enabled_count;
                    } else {
                        ++disabled_count;
                    }
                    loggers::DEVICE::DEBUG(
                        "PLIC context: phandle=%u hwirq=%u hart=%u ctx=%u enabled=%d virq=%llu",
                        phandle, static_cast<unsigned>(hwirq), context.hart_id,
                        static_cast<unsigned>(context.ctx_id), context.enabled,
                        static_cast<unsigned long long>(context.external_virq));
                    contexts.push_back(context);
                }

                loggers::DEVICE::INFO(
                    "PLIC context 解析完成: total=%u enabled=%u disabled=%u",
                    static_cast<unsigned>(contexts.size()),
                    static_cast<unsigned>(enabled_count),
                    static_cast<unsigned>(disabled_count));
                return contexts;
            }

            /**
             * @brief 构造单个 PLIC context.
             *
             * @param index interrupts-extended 中的原始条目序号.
             * @param phandle 父中断控制器 phandle.
             * @param hwirq 条目中的硬件中断号.
             * @param irqman 全局中断管理器.
             * @return Result<riscv::Plic::Context> context 构造结果.
             */
            [[nodiscard]]
            Result<riscv::Plic::Context> build_plic_context(
                size_t index, phandle_t phandle, hwirq_t hwirq,
                driver::IrqManager &irqman) const noexcept {
                auto *intc_node = _provider->config().get_node_by_phandle(phandle);
                if (intc_node == nullptr || intc_node->parent == nullptr) {
                    loggers::DEVICE::ERROR(
                        "PLIC context 解析失败: phandle=%u 找不到对应 CPU intc 节点",
                        phandle);
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }

                auto parsed_res = parse_cpu_node(*intc_node->parent);
                if (!parsed_res.has_value()) {
                    loggers::DEVICE::ERROR(
                        "PLIC context 解析失败: phandle=%u CPU 节点解析失败 err=%s",
                        phandle, to_cstring(parsed_res.error()));
                    propagate_return(parsed_res);
                }

                auto hart_id = parsed_res.value().id;
                auto ctx_id = static_cast<riscv::Plic::ctx_t>(
                    hart_id * 2 + index % 2);
                riscv::Plic::Context context{
                    .hart_id       = hart_id,
                    .external_virq = 0,
                    .ctx_id        = ctx_id,
                    .enabled       = false,
                    .completed     = true,
                };

                auto parent_domain_res =
                    _provider->resolve_irq_domain_view(phandle, irqman);
                if (!parent_domain_res.has_value()) {
                    loggers::DEVICE::DEBUG(
                        "PLIC context 保留为 disabled: phandle=%u hart=%u ctx=%u err=%s",
                        phandle, hart_id, static_cast<unsigned>(ctx_id),
                        to_cstring(parent_domain_res.error()));
                    return context;
                }

                auto virq_res = irqman.allocate_virq(
                    parent_domain_res.value().get().id(), hwirq);
                if (!virq_res.has_value()) {
                    loggers::DEVICE::DEBUG(
                        "PLIC context 保留为 disabled: phandle=%u hart=%u ctx=%u virq 分配失败 err=%s",
                        phandle, hart_id, static_cast<unsigned>(ctx_id),
                        to_cstring(virq_res.error()));
                    return context;
                }

                context.external_virq = virq_res.value();
                context.enabled       = true;
                return context;
            }

            const FDTProvider *_provider = nullptr;
        };

 #endif
    }  // namespace

    /**
     * @brief 构造 FDT 设备提供者并初始化默认 IRQ 工厂.
     */
    FDTProvider::FDTProvider(void *dtb) {
        make_config(dtb, _config);
        init_device_factories();
        init_irq_factories();
    }

    /**
     * @brief 向普通设备工厂注册表登记默认 FDT 设备工厂.
     */
    void FDTProvider::init_device_factories() noexcept {
        virtio::init_virtio_blk_factory();
    }

    /**
     * @brief 向 IRQ 工厂注册表登记默认 FDT IRQ 工厂.
     */
    void FDTProvider::init_irq_factories() noexcept {
        if (!driver::DriverModel::initialized()) {
            return;
        }

#if defined(__ARCH_riscv64__)
        auto *root_factory  = new RiscVIntCIrqFactory(&_cpu_intc_candidates);
        auto *clint_factory = new ClintIrqFactory(&_local_intc_map);
        auto *plic_factory  = new PlicIrqFactory();
        if (root_factory == nullptr || clint_factory == nullptr ||
            plic_factory == nullptr)
        {
            return;
        }

        root_factory->bind_provider(*this);
        clint_factory->bind_provider(*this);
        plic_factory->bind_provider(*this);

        [[maybe_unused]] auto root_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(root_factory));
        [[maybe_unused]] auto clint_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(clint_factory));
        [[maybe_unused]] auto plic_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(plic_factory));
#endif
    }

    Result<void> FDTProvider::register_irq_domain(
        phandle_t phandle, const driver::IrqDomain &domain) const {
        if (phandle == 0) {
            loggers::DEVICE::ERROR("拒绝登记无效中断控制器 phandle=0");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto it = _irq_domains.find(phandle);
        if (it != _irq_domains.end()) {
            if (it->second != domain.id()) {
                loggers::DEVICE::ERROR(
                    "中断控制器 phandle=%u 已映射到 domain=%u, 无法改写为 "
                    "domain=%u",
                    phandle, it->second, domain.id());
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
            loggers::DEVICE::DEBUG(
                "中断控制器 phandle=%u 已登记到 domain=%u, 跳过重复登记",
                phandle, domain.id());
            void_return();
        }

        _irq_domains[phandle] = domain.id();
        loggers::DEVICE::DEBUG("登记中断控制器 phandle=%u -> domain=%u",
                               phandle, domain.id());
        void_return();
    }

    Result<driver::IrqDomain &> FDTProvider::resolve_irq_domain(
        phandle_t phandle, driver::IrqManager &irqman) const {
        auto it = _irq_domains.find(phandle);
        if (it == _irq_domains.end()) {
            loggers::DEVICE::ERROR("未找到 phandle=%u 对应的中断域映射",
                                   phandle);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        loggers::DEVICE::DEBUG("解析中断域: phandle=%u -> domain=%u", phandle,
                               it->second);
        return irqman.get_domain(it->second);
    }

    Result<size_t> FDTProvider::interrupt_cells_for_controller(
        phandle_t controller_phandle) const {
        Node *controller = _config.get_node_by_phandle(controller_phandle);
        if (controller == nullptr) {
            loggers::DEVICE::ERROR("找不到 phandle=%u 对应的中断控制器节点",
                                   controller_phandle);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto cells_it = controller->properties.find(INTERRUPT_CELLS_PROP);
        if (cells_it == controller->properties.end()) {
            loggers::DEVICE::ERROR("中断控制器节点 %s 缺少 %s",
                                   controller->name.c_str(),
                                   INTERRUPT_CELLS_PROP);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        size_t cell_count =
            static_cast<size_t>(cells_it->second->as_integral());
        if (cell_count != 1) {
            loggers::DEVICE::ERROR(
                "中断控制器节点 %s 的 %s=%u, 当前仅支持单 cell 中断编码",
                controller->name.c_str(), INTERRUPT_CELLS_PROP,
                static_cast<unsigned>(cell_count));
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return cell_count;
    }

    Result<phandle_t> FDTProvider::resolve_interrupt_parent(
        const Node &node) const {
        for (const Node *current = &node; current != nullptr;
             current             = current->parent)
        {
            auto parent_it = current->properties.find(INTERRUPT_PARENT_PROP);
            if (parent_it == current->properties.end()) {
                continue;
            }

            phandle_t phandle = parent_it->second->as_phandle();
            if (phandle == 0) {
                loggers::DEVICE::ERROR("节点 %s 的 interrupt-parent 为 0",
                                       current->name.c_str());
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return phandle;
        }

        loggers::DEVICE::ERROR("节点 %s 及其祖先均未声明 interrupt-parent",
                               node.name.c_str());
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<std::vector<FDTProvider::InterruptRef>>
    FDTProvider::parse_interrupts_extended(const Node &node) const {
        auto prop_it = node.properties.find(INTERRUPT_EXT_PROP);
        if (prop_it == node.properties.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto cells = parse_u32_cells(*prop_it->second);
        if (cells.empty()) {
            loggers::DEVICE::ERROR("节点 %s 的 interrupts-extended 为空或非法",
                                   node.name.c_str());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        std::vector<InterruptRef> refs;
        refs.reserve(cells.size() / 2);

        for (size_t offset = 0; offset < cells.size();) {
            phandle_t phandle = cells[offset++];
            if (phandle == 0) {
                loggers::DEVICE::ERROR(
                    "节点 %s 的 interrupts-extended 含有 phandle=0",
                    node.name.c_str());
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            auto cell_count_res = interrupt_cells_for_controller(phandle);
            propagate(cell_count_res);
            size_t cell_count = cell_count_res.value();
            if (offset + cell_count > cells.size()) {
                loggers::DEVICE::ERROR(
                    "节点 %s 的 interrupts-extended 长度不足, phandle=%u "
                    "需要 %u 个中断 cell",
                    node.name.c_str(), phandle,
                    static_cast<unsigned>(cell_count));
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            hwirq_t hwirq = static_cast<hwirq_t>(cells[offset]);
            offset += cell_count;

            refs.emplace_back(phandle, hwirq);
            loggers::DEVICE::DEBUG(
                "解析 interrupts-extended: node=%s phandle=%u hwirq=%u",
                node.name.c_str(), phandle, static_cast<unsigned>(hwirq));
        }

        return refs;
    }

    Result<std::vector<FDTProvider::InterruptRef>>
    FDTProvider::parse_interrupts(const Node &node) const {
        auto parent_res = resolve_interrupt_parent(node);
        propagate(parent_res);
        phandle_t parent_phandle = parent_res.value();

        auto cell_count_res = interrupt_cells_for_controller(parent_phandle);
        propagate(cell_count_res);
        size_t cell_count = cell_count_res.value();

        auto prop_it = node.properties.find(INTERRUPTS_PROP);
        if (prop_it == node.properties.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto cells = parse_u32_cells(*prop_it->second);
        if (cells.empty() || cells.size() % cell_count != 0) {
            loggers::DEVICE::ERROR("节点 %s 的 interrupts 属性长度非法",
                                   node.name.c_str());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        std::vector<InterruptRef> refs;
        refs.reserve(cells.size() / cell_count);
        for (size_t offset = 0; offset < cells.size(); offset += cell_count) {
            hwirq_t hwirq = static_cast<hwirq_t>(cells[offset]);
            refs.emplace_back(parent_phandle, hwirq);
            loggers::DEVICE::DEBUG(
                "解析 interrupts: node=%s parent=%u hwirq=%u",
                node.name.c_str(), parent_phandle,
                static_cast<unsigned>(hwirq));
        }

        return refs;
    }

    Result<std::vector<virq_t>>
    FDTProvider::resolve_interrupt_refs_to_virqs(
        const std::vector<InterruptRef> &refs,
        driver::IrqManager &irqman) const {
        std::vector<virq_t> virqs;
        virqs.reserve(refs.size());

        for (const auto &[phandle, hwirq] : refs) {
            auto domain_res = resolve_irq_domain(phandle, irqman);
            propagate(domain_res);

            if (domain_res.value().get().id() ==
                    static_cast<domain_t>(phandle) &&
                hwirq == static_cast<hwirq_t>(-1))
            {
                loggers::DEVICE::DEBUG(
                    "跳过无效中断引用: phandle=%u hwirq=%u",
                    phandle, static_cast<unsigned>(hwirq));
                continue;
            }

            auto virq_res =
                irqman.allocate_virq(domain_res.value().get().id(), hwirq);
            propagate(virq_res);

            virqs.push_back(virq_res.value());
            loggers::DEVICE::DEBUG(
                "解析中断引用到 virq: phandle=%u domain=%u hwirq=%u virq=%llu",
                phandle, domain_res.value().get().id(),
                static_cast<unsigned>(hwirq),
                static_cast<unsigned long long>(virq_res.value()));
        }

        return virqs;
    }

    Result<std::vector<virq_t>> FDTProvider::parse_interrupt_virqs(
        const Node &node, driver::IrqManager &irqman) const {
        auto ext_refs_res = parse_interrupts_extended(node);
        if (ext_refs_res.has_value()) {
            return resolve_interrupt_refs_to_virqs(ext_refs_res.value(),
                                                   irqman);
        }
        if (ext_refs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(ext_refs_res);
        }

        auto refs_res = parse_interrupts(node);
        propagate(refs_res);
        return resolve_interrupt_refs_to_virqs(refs_res.value(), irqman);
    }

    void FDTProvider::append_as_regions(std::vector<MemRegion> &regions,
                                        const RegionCells &cells,
                                        const Property &prop,
                                        MemRegion::MemoryStatus status) const {
        auto new_regions = prop.as_regions(cells);
        for (const auto &area : new_regions) {
            regions.emplace_back(MemRegion{.status = status, .area = area});
        }
    }

    bool FDTProvider::is_memory_node(Node &node) const {
        return is_string_prop_equal(node, DEVICE_TYPE_PROP, MEMORY_DEVICE_TYPE);
    }

    bool FDTProvider::node_is_simple_bus(
        const device::DeviceNode &node) const noexcept {
        return node.is_compatible_with("simple-bus") >= 0;
    }

    Result<device::DeviceNode *> FDTProvider::make_device_node(
        const Node &node, device::DeviceModel &model) const {
        return model.register_device_node(util::owner<device::DeviceNode *>(
            new FDTDeviceNode(*this, _config, node)));
    }

    template <typename Fn>
    void FDTProvider::scan_visible_nodes(const Node &root, Fn &&handler) const {
        for (const auto *child : sorted_children(root)) {
            if (!node_status_enabled(*child)) {
                continue;
            }
            auto compatible_it = child->properties.find(COMPATIBLE_PROP);
            if (compatible_it == child->properties.end() ||
                compatible_it->second->as_string_list().empty())
            {
                continue;
            }
            FDTDeviceNode probe(*this, _config, *child);
            auto &device_node = static_cast<device::DeviceNode &>(probe);
            if (node_is_simple_bus(device_node)) {
                scan_visible_nodes(*child, std::forward<Fn>(handler));
                continue;
            }
            handler(*child);
        }
    }

    void FDTProvider::register_memory_regions(
        device::DeviceModel &model) const {
        (void)model;
    }

    void FDTProvider::register_platform(device::DeviceModel &model) const {
        if (!_config.root) {
            loggers::DEVICE::WARN("设备树根节点不存在, 无法获取平台信息");
            return;
        }

        Node *cpus_node = _config.get_node_by_path(CPUS_PATH);
        if (cpus_node == nullptr || !node_status_enabled(*cpus_node)) {
            loggers::DEVICE::WARN(
                "设备树中缺少 /cpus 节点或其状态不可用, 无法获取平台信息");
            return;
        }

        auto freq_prop_it = cpus_node->properties.find(TIMEBASE_FREQ_PROP);
        if (freq_prop_it == cpus_node->properties.end()) {
            loggers::DEVICE::WARN(
                "节点 /cpus 缺少 timebase-frequency 属性, 无法构造平台时钟源");
            return;
        }

        auto timebase_freq = freq_prop_it->second->as_integral();
        if (timebase_freq == 0) {
            loggers::DEVICE::ERROR(
                "节点 /cpus 的 timebase-frequency 为 0, 无法创建平台");
            return;
        }

#if defined(__ARCH_riscv64__)
        auto freq = units::frequency::from_hz(timebase_freq);
        model.set_platform(util::owner<device::Platform *>(
            new riscv::Riscv64Platform(freq)));
        loggers::DEVICE::INFO("已创建 Riscv64Platform, timebase-frequency=%lluHz",
                              static_cast<unsigned long long>(freq.to_hz()));
#else
        (void)timebase_freq;
#endif
    }

    void FDTProvider::register_cpus(device::DeviceModel &model) const {
        auto &cpus = model.cpus();
        _irq_domains.clear();
        _cpu_intc_candidates.clear();
        _local_intc_map.clear();

        // 清理旧的 CPU 组信息.
        loggers::DEVICE::DEBUG("开始更新 CPU 组信息");
        cpus.cleanup();
        cpus.topology.cleanup();

        // 解析 /cpus 根节点.
        if (!_config.root) {
            loggers::DEVICE::WARN("设备树根节点不存在, 无法获取 CPU 信息");
            return;
        }

        Node *cpus_node = _config.get_node_by_path(CPUS_PATH);
        if (cpus_node == nullptr || !node_status_enabled(*cpus_node)) {
            loggers::DEVICE::WARN(
                "设备树中缺少 /cpus 节点或其状态不可用, 无法获取 CPU 信息");
            return;
        }

        auto *platform = model.platform();
        if (platform == nullptr) {
            loggers::DEVICE::ERROR("平台对象不可用, 无法构建 CPU 信息");
            return;
        }
        auto *clock_source = platform->clock_source();
        if (clock_source == nullptr) {
            loggers::DEVICE::ERROR("平台 ClockSource 不可用, 无法构建 CPU 信息");
            return;
        }
        auto cpu_frequency = clock_source->frequency();

        std::vector<ParsedCpu> parsed_cpus;
        parsed_cpus.reserve(cpus_node->children.size());
        std::unordered_map<fdt::phandle_t, device::cpuid_t> cpu_phandle_map;
        // 扫描并解析每个 CPU 节点.
        for (const auto *child : sorted_children(*cpus_node)) {
            auto parsed_res = parse_cpu_node(*child);
            if (!parsed_res.has_value()) {
                if (parsed_res.error() == ErrCode::BUSY ||
                    parsed_res.error() == ErrCode::INVALID_PARAM)
                {
                    continue;
                }
                loggers::DEVICE::ERROR("解析 CPU 节点 %s 失败: %s",
                                       child->name.c_str(),
                                       to_cstring(parsed_res.error()));
                return;
            }

            const auto &parsed = parsed_res.value();
            if (cpu_phandle_map.contains(parsed.cpu_phandle)) {
                loggers::DEVICE::ERROR("CPU phandle=%u 被多个 CPU 重复使用",
                                       parsed.cpu_phandle);
                return;
            }
            if (_local_intc_map.contains(parsed.local_intc_phandle)) {
                loggers::DEVICE::ERROR(
                    "本地中断 phandle=%u 被多个 CPU 重复使用",
                    parsed.local_intc_phandle);
                return;
            }
            cpu_phandle_map[parsed.cpu_phandle]        = parsed.id;
            _local_intc_map[parsed.local_intc_phandle] = parsed.id;
            parsed_cpus.push_back(parsed);
        }

        if (parsed_cpus.empty()) {
            loggers::DEVICE::WARN("未在 /cpus 下解析到可用 CPU");
            return;
        }

        std::ranges::sort(parsed_cpus,
                          [](const ParsedCpu &lhs, const ParsedCpu &rhs) {
                              return lhs.id < rhs.id;
                          });

        // 收集每个 CPU 的本地中断节点, 后续由 register_intcs() 统一建域.
        for (const auto &parsed : parsed_cpus) {
            Node *intc_node =
                _config.get_node_by_phandle(parsed.cpu_intc_phandle);
            if (intc_node == nullptr) {
                loggers::DEVICE::ERROR(
                    "CPU %u 的本地中断节点不存在: phandle=%u", parsed.id,
                    parsed.cpu_intc_phandle);
                return;
            }
            _cpu_intc_candidates.push_back(CpuIntcDescriptor{
                .node    = intc_node,
                .hart_id = parsed.id,
                .identifier =
                    static_cast<intc_t>(parsed.cpu_intc_phandle),
                .name = "riscv,cpu-intc",
            });
        }

        // 构造 CPU 对象列表.
        std::vector<device::cpuid_t> cpu_ids;
        cpu_ids.reserve(parsed_cpus.size());
        for (const auto &parsed : parsed_cpus) {
            auto cpu_res = device::RiscV64Cpu::Builder()
                               .id(parsed.id)
                               .model(parsed.model)
                               .frequency(cpu_frequency)
                               .isa_string(parsed.isa_string)
                               .mmu_type(parsed.mmu_type)
                               .local_intc(static_cast<intc_t>(
                                   parsed.cpu_intc_phandle))
                               .build();
            if (!cpu_res.has_value()) {
                loggers::DEVICE::ERROR("构建 CPU %u 失败: %s", parsed.id,
                                       to_cstring(cpu_res.error()));
                return;
            }
            cpu_ids.push_back(parsed.id);
            cpus.cpus.emplace_back(cpu_res.value().get());
        }

        auto cpu_map_it = cpus_node->children.find(CPU_MAP_NODE);
        Result<device::CpuTopology> topology_res =
            cpu_map_it != cpus_node->children.end()
                ? build_cpu_map_topology(*cpu_map_it->second, cpu_phandle_map,
                                         cpu_ids)
                : build_default_topology(cpu_ids);

        if (!topology_res.has_value()) {
            loggers::DEVICE::WARN("构建 cpu-map 拓扑失败: %s, 降级为默认拓扑",
                                  to_cstring(topology_res.error()));
            topology_res = build_default_topology(cpu_ids);
        }
        if (!topology_res.has_value()) {
            loggers::DEVICE::ERROR("构建默认 CPU 拓扑失败: %s",
                                   to_cstring(topology_res.error()));
            return;
        }

        cpus.topology = std::move(topology_res.value());
        loggers::DEVICE::INFO(
            "CPU 信息更新完成: freq=%lluHz count=%u topo_cpus=%u",
            static_cast<unsigned long long>(cpu_frequency.to_hz()),
            static_cast<unsigned>(cpus.cpus.size()),
            static_cast<unsigned>(cpus.topology.logical_cpus().size()));
    }

    void FDTProvider::register_nodes(device::DeviceModel &model) const {
        if (!_config.root) {
            return;
        }

        scan_visible_nodes(*_config.root, [this, &model](const Node &node) {
            auto node_res = make_device_node(node, model);
            if (!node_res.has_value()) {
                loggers::DEVICE::ERROR("登记 DeviceNode 失败: node=%s err=%s",
                                       node.name.c_str(),
                                       to_cstring(node_res.error()));
            }
        });

        // /cpus 下的本地中断端点不会经过通用可见节点扫描, 这里补登记一次.
        for (const auto &candidate : _cpu_intc_candidates) {
            if (candidate.node == nullptr) {
                continue;
            }

            bool already_registered = false;
            for (const auto &node_owner : model.device_nodes()) {
                if (node_owner.get() == nullptr) {
                    continue;
                }
                auto *fdt_node =
                    static_cast<const FDTDeviceNode *>(node_owner.get());
                if (&fdt_node->raw_node() == candidate.node) {
                    already_registered = true;
                    break;
                }
            }
            if (already_registered) {
                continue;
            }

            auto node_res = make_device_node(*candidate.node, model);
            if (!node_res.has_value()) {
                loggers::DEVICE::ERROR(
                    "登记 CPU local intc DeviceNode 失败: node=%s err=%s",
                    candidate.node->name.c_str(), to_cstring(node_res.error()));
                return;
            }
        }
    }

    void FDTProvider::register_intcs(device::DeviceModel &model) const {
        auto register_node =
            [this, &model](const FDTDeviceNode &fdt_node) -> Result<void> {
            auto *irq_factory =
                driver::DriverModel::inst().irq_factories().find(fdt_node);
            if (irq_factory == nullptr) {
                void_return();
            }

            loggers::DEVICE::DEBUG("发现 IRQ 设备节点: node=%s compatible=%s",
                                   fdt_node.raw_node().name.c_str(),
                                   irq_factory->compatible().data());

            auto device_res = driver::DriverModel::inst().create_irq_driver(
                const_cast<FDTDeviceNode *>(&fdt_node));
            propagate(device_res);
            void_return();
        };

        // 第一轮: 先注册 CPU 本地中断端点, 建立 root domain 映射.
        for (const auto &candidate : _cpu_intc_candidates) {
            if (candidate.node == nullptr) {
                continue;
            }
            for (const auto &node_owner : model.device_nodes()) {
                if (node_owner.get() == nullptr) {
                    continue;
                }
                auto *fdt_node =
                    static_cast<const FDTDeviceNode *>(node_owner.get());
                if (&fdt_node->raw_node() != candidate.node) {
                    continue;
                }
                auto register_res = register_node(*fdt_node);
                if (!register_res.has_value()) {
                    loggers::DEVICE::ERROR(
                        "注册 CPU IRQ 控制器失败: node=%s err=%s",
                        fdt_node->raw_node().name.c_str(),
                        to_cstring(register_res.error()));
                    return;
                }
                break;
            }
        }

        // 第二轮: 再注册其它 IRQ 控制器.
        for (const auto &node_owner : model.device_nodes()) {
            if (node_owner.get() == nullptr) {
                continue;
            }
            auto *fdt_node =
                static_cast<const FDTDeviceNode *>(node_owner.get());
            bool is_cpu_intc = false;
            for (const auto &candidate : _cpu_intc_candidates) {
                if (candidate.node == &fdt_node->raw_node()) {
                    is_cpu_intc = true;
                    break;
                }
            }
            if (is_cpu_intc) {
                continue;
            }

            auto register_res = register_node(*fdt_node);
            if (!register_res.has_value()) {
                loggers::DEVICE::ERROR("注册 IRQ 控制器失败: node=%s err=%s",
                                       fdt_node->raw_node().name.c_str(),
                                       to_cstring(register_res.error()));
                return;
            }
        }
    }

    void FDTProvider::register_clock_virq(
        device::DeviceModel &model) const noexcept {
        loggers::DEVICE::DEBUG(
            "FDTProvider clock_virq=%llu",
            static_cast<unsigned long long>(model.clock_virq()));
    }

    void FDTProvider::register_device(device::DeviceModel &model) const {
        register_memory_regions(model);
        register_platform(model);
        register_cpus(model);
        register_nodes(model);
        register_intcs(model);
        register_clock_virq(model);
    }
}  // namespace fdt
