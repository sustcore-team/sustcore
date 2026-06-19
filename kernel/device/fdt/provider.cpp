/**
 * @file provider.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT Provider 实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/device/cpu.h>
#include <arch/riscv64/device/fdt/irq_factories.h>
#include <arch/riscv64/device/platform.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/device/cpu.h>
#include <arch/loongarch64/device/fdt/irq_factories.h>
#include <arch/loongarch64/device/platform.h>
#endif
#include <device/fdt/decode.h>
#include <device/fdt/device_node.h>
#include <device/fdt/internal.h>
#include <device/ic_graph.h>
#include <driver/model.h>
#include <driver/virtio/virtio-blk.h>

#include <algorithm>
#include <limits>
#include <ranges>
#include <unordered_set>

namespace {
    constexpr uint64_t LOONGARCH_TIMER_FREQ_HZ = 100000000ULL;
}  // namespace

namespace fdt {
    const driver::IIrqChipFactory *find_irq_factory_for_node(
        const driver::DriverModel &driver_model,
        const device::DeviceNode &node) noexcept {
        for (const auto *factory : driver_model.irq_factories().factories()) {
            if (factory == nullptr) {
                continue;
            }
            auto match = driver_model.irq_factories().match(*factory, node);
            if (match.matched &&
                factory->probe(node, device::DeviceModel::inst(),
                               match.driver_flag))
            {
                return factory;
            }
        }
        return nullptr;
    }

    Result<std::vector<phandle_t>> interrupt_parent_phandles_for_node(
        const FDTProvider &provider, const Node &node) {
        std::vector<phandle_t> parents;

        auto ext_refs_res = provider.parse_interrupts_extended_view(node);
        if (ext_refs_res.has_value()) {
            for (const auto &ref : ext_refs_res.value()) {
                parents.push_back(ref.phandle);
            }
        } else if (ext_refs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(ext_refs_res);
        } else {
            auto refs_res = provider.parse_interrupts(node);
            if (refs_res.has_value()) {
                for (const auto &ref : refs_res.value()) {
                    parents.push_back(ref.phandle);
                }
            } else if (refs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
                propagate_return(refs_res);
            }
        }

        std::sort(parents.begin(), parents.end());
        parents.erase(std::unique(parents.begin(), parents.end()), parents.end());
        return parents;
    }

    std::vector<device::cpuid_t> target_harts_from_interrupt_refs(
        const char *node_name,
        const std::vector<ParsedInterruptRef> &refs,
        const LocalInterruptTargetMap &local_intc_map) {
        std::vector<device::cpuid_t> target_harts;
        target_harts.reserve(refs.size());

        for (const auto &ref : refs) {
            auto cpu_it = local_intc_map.find(ref.phandle);
            if (cpu_it == local_intc_map.end()) {
                loggers::DEVICE::DEBUG(
                    "%s 的中断引用未匹配到本地 hart: phandle=%u hwirq=%u",
                    node_name, ref.phandle,
                    static_cast<unsigned>(ref.hwirq));
                continue;
            }
            target_harts.push_back(cpu_it->second);
        }

        normalize_target_harts(target_harts);
        return target_harts;
    }

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

    Result<std::vector<device::cpuid_t>> build_cpu_map_subtree(
        device::CpuTopologyBuilder &builder, const Node &map_node,
        device::topo_t parent_id, device::topo_t &next_topo_id,
        const std::unordered_map<phandle_t, device::cpuid_t> &cpu_phandle_map) {
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

        std::vector<device::cpuid_t> aggregated;
        auto cpu_it = map_node.properties.find(CPU_PROP);
        if (cpu_it != map_node.properties.end()) {
            phandle_t cpu_phandle = cpu_it->second->as_phandle();
            auto cpu_map_it       = cpu_phandle_map.find(cpu_phandle);
            if (cpu_map_it == cpu_phandle_map.end()) {
                loggers::DEVICE::ERROR(
                    "cpu-map 节点 %s 引用了未知 CPU phandle=%u",
                    map_node.name.c_str(), cpu_phandle);
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            aggregated.push_back(cpu_map_it->second);
        }

        for (const auto *child : sorted_children(map_node)) {
            auto child_res = build_cpu_map_subtree(
                builder, *child, node_id, next_topo_id, cpu_phandle_map);
            propagate(child_res);
            const auto &child_cpus = child_res.value();
            aggregated.insert(aggregated.end(), child_cpus.begin(),
                              child_cpus.end());
        }

        std::sort(aggregated.begin(), aggregated.end());
        aggregated.erase(std::unique(aggregated.begin(), aggregated.end()),
                         aggregated.end());

        auto cpus_res = builder.cpus(node_id, aggregated);
        propagate(cpus_res);
        return aggregated;
    }

    Result<device::CpuTopology> build_default_topology(
        const std::vector<device::cpuid_t> &cpu_ids) {
        device::CpuTopologyBuilder builder;

        builder.root(device::CpuTopoLevel::CLUSTER, 0);

        auto root_res = builder.cpus(0, cpu_ids);
        propagate(root_res);

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

    Result<device::CpuTopology> build_cpu_map_topology(
        const Node &cpu_map_node,
        const std::unordered_map<phandle_t, device::cpuid_t> &cpu_phandle_map,
        const std::vector<device::cpuid_t> &all_cpu_ids) {
        auto top_children = sorted_children(cpu_map_node);
        if (top_children.empty()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        device::CpuTopologyBuilder builder;
        device::topo_t next_topo_id = 1;

        if (top_children.size() == 1) {
            auto root_level = topo_level_from_name(top_children.front()->name);
            if (!root_level.has_value()) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            builder.root(*root_level, 0);

            std::vector<device::cpuid_t> aggregated;
            auto cpu_it = top_children.front()->properties.find(CPU_PROP);
            if (cpu_it != top_children.front()->properties.end()) {
                phandle_t cpu_phandle = cpu_it->second->as_phandle();
                auto cpu_map_it       = cpu_phandle_map.find(cpu_phandle);
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

            std::sort(aggregated.begin(), aggregated.end());
            aggregated.erase(
                std::unique(aggregated.begin(), aggregated.end()),
                aggregated.end());
            auto root_res = builder.cpus(0, aggregated);
            propagate(root_res);
        } else {
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

    Result<ParsedCpu> parse_cpu_node(const Node &cpu_node) {
        if (!node_status_enabled(cpu_node)) {
            unexpect_return(ErrCode::BUSY);
        }
        if (!is_string_prop_equal(cpu_node, DEVICE_TYPE_PROP, CPU_DEVICE_TYPE))
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

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

        std::string isa_string{};
        std::string mmu_type{};
        phandle_t local_intc_phandle = 0;
        phandle_t cpu_intc_phandle   = 0;

#if defined(__ARCH_riscv64__)
        auto isa_it = cpu_node.properties.find(RISCV_ISA_PROP);
        if (isa_it == cpu_node.properties.end()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少 riscv,isa 属性",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        isa_string = isa_it->second->as_string();
        mmu_type   = cpu_node.properties.contains(MMU_TYPE_PROP)
                         ? cpu_node.properties.at(MMU_TYPE_PROP)->as_string()
                         : "";

        auto local_intc = find_local_intc_phandle(cpu_node);
        if (!local_intc.has_value()) {
            loggers::DEVICE::ERROR("CPU 节点 %s 缺少本地 interrupt-controller",
                                   cpu_node.name.c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        local_intc_phandle = *local_intc;
        cpu_intc_phandle   = *local_intc;
#elif defined(__ARCH_loongarch64__)
        auto *root = cpu_node.parent != nullptr ? cpu_node.parent->parent : nullptr;
        auto *cpuic_node =
            root != nullptr && root->children.contains("cpuic")
                ? root->children.at("cpuic").get()
                : nullptr;
        if (cpuic_node == nullptr || cpuic_node->phandle == 0) {
            loggers::DEVICE::ERROR("LoongArch CPU 缺少可用 cpuic 节点");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        local_intc_phandle = cpuic_node->phandle;
        cpu_intc_phandle   = cpuic_node->phandle;
#endif

        ParsedCpu parsed{
            .id                 = static_cast<device::cpuid_t>(*reg_value),
            .model              = fallback_cpu_model(cpu_node),
            .isa_string         = std::move(isa_string),
            .mmu_type           = std::move(mmu_type),
            .cpu_phandle        = cpu_node.phandle,
            .local_intc_phandle = local_intc_phandle,
            .cpu_intc_phandle   = cpu_intc_phandle,
        };
        return parsed;
    }

    FDTProvider::FDTProvider(void *dtb) {
        make_config(dtb, _config);
        init_device_factories();
        init_irq_factories();
    }

    void FDTProvider::init_device_factories() noexcept {
        virtio::init_virtio_blk_factory();
    }

    void FDTProvider::init_irq_factories() noexcept {
        if (!driver::DriverModel::initialized()) {
            return;
        }

#if defined(__ARCH_riscv64__)
        riscv::fdt::register_irq_factories(*this);
#elif defined(__ARCH_loongarch64__)
        la64::fdt::register_irq_factories(*this);
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
                    "中断控制器 phandle=%u 已映射到 domain=%u, 无法改写为 domain=%u",
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
        if (cell_count != 1 && cell_count != 2) {
            loggers::DEVICE::ERROR(
                "中断控制器节点 %s 的 %s=%u, 当前仅支持 1 或 2 cell 中断编码",
                controller->name.c_str(), INTERRUPT_CELLS_PROP,
                static_cast<unsigned>(cell_count));
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return cell_count;
    }

    std::optional<phandle_t> FDTProvider::maybe_interrupt_parent(
        const Node &node) const noexcept {
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
                return std::nullopt;
            }
            return phandle;
        }

        return std::nullopt;
    }

    Result<phandle_t> FDTProvider::resolve_interrupt_parent(
        const Node &node) const {
        auto parent = maybe_interrupt_parent(node);
        if (parent.has_value()) {
            return parent.value();
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
                    "节点 %s 的 interrupts-extended 长度不足, phandle=%u 需要 %u 个中断 cell",
                    node.name.c_str(), phandle,
                    static_cast<unsigned>(cell_count));
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            hwirq_t hwirq = static_cast<hwirq_t>(cells[offset]);
            std::optional<driver::IrqTrigger> trigger = std::nullopt;
            if (cell_count == 2) {
                auto trigger_opt = decode_trigger_cell(cells[offset + 1]);
                if (!trigger_opt.has_value()) {
                    loggers::DEVICE::ERROR(
                        "节点 %s 的 interrupts-extended 含有非法触发方式: phandle=%u raw=%u",
                        node.name.c_str(), phandle,
                        static_cast<unsigned>(cells[offset + 1]));
                } else {
                    trigger = trigger_opt;
                }
            }
            offset += cell_count;

            refs.push_back(FDTProvider::InterruptRef{
                .phandle = phandle,
                .hwirq   = hwirq,
                .trigger = trigger,
            });
            loggers::DEVICE::DEBUG(
                "解析 interrupts-extended: node=%s phandle=%u hwirq=%u trigger=%d",
                node.name.c_str(), phandle, static_cast<unsigned>(hwirq),
                trigger.has_value() ? static_cast<int>(*trigger) : -1);
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
            std::optional<driver::IrqTrigger> trigger = std::nullopt;
            if (cell_count == 2) {
                auto trigger_opt = decode_trigger_cell(cells[offset + 1]);
                if (!trigger_opt.has_value()) {
                    loggers::DEVICE::ERROR(
                        "节点 %s 的 interrupts 含有非法触发方式: parent=%u raw=%u",
                        node.name.c_str(), parent_phandle,
                        static_cast<unsigned>(cells[offset + 1]));
                } else {
                    trigger = trigger_opt;
                }
            }
            refs.push_back(FDTProvider::InterruptRef{
                .phandle = parent_phandle,
                .hwirq   = hwirq,
                .trigger = trigger,
            });
            loggers::DEVICE::DEBUG(
                "解析 interrupts: node=%s parent=%u hwirq=%u trigger=%d",
                node.name.c_str(), parent_phandle,
                static_cast<unsigned>(hwirq),
                trigger.has_value() ? static_cast<int>(*trigger) : -1);
        }

        return refs;
    }

    Result<domain_t> FDTProvider::resolve_interrupt_domain(
        phandle_t phandle) const {
        auto it = _irq_domains.find(phandle);
        if (it == _irq_domains.end()) {
            loggers::DEVICE::ERROR("未找到 phandle=%u 对应的中断域映射",
                                   phandle);
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return it->second;
    }

    Result<std::vector<driver::virq_t>>
    FDTProvider::resolve_interrupt_refs_to_virqs(
        const std::vector<InterruptRef> &refs,
        driver::IrqManager &irqman) const {
        std::vector<driver::virq_t> virqs;
        virqs.reserve(refs.size());

        for (const auto &ref : refs) {
            auto domain_res = resolve_irq_domain(ref.phandle, irqman);
            propagate(domain_res);

            auto virq_res =
                irqman.allocate_virq(domain_res.value().get().id(), ref.hwirq);
            propagate(virq_res);

            if (ref.trigger.has_value()) {
                auto trigger_res =
                    irqman.set_trigger(virq_res.value(), *ref.trigger);
                if (!trigger_res.has_value() &&
                    trigger_res.error() != ErrCode::NOT_SUPPORTED)
                {
                    propagate_return(trigger_res);
                }
            }

            virqs.push_back(virq_res.value());
        }
        return virqs;
    }

    Result<std::vector<driver::virq_t>> FDTProvider::parse_interrupt_virqs(
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

    Result<std::vector<device::RawIrqSpec>>
    FDTProvider::resolve_interrupt_refs_to_specs(
        const std::vector<InterruptRef> &refs) const {
        std::vector<device::RawIrqSpec> specs;
        specs.reserve(refs.size());

        for (const auto &ref : refs) {
            auto domain_res = resolve_interrupt_domain(ref.phandle);
            propagate(domain_res);
            specs.push_back(device::RawIrqSpec{
                .domain  = domain_res.value(),
                .hwirq   = ref.hwirq,
                .trigger = ref.trigger,
            });
        }

        return specs;
    }

    Result<std::vector<device::RawIrqSpec>> FDTProvider::parse_interrupt_specs(
        const Node &node) const {
        auto ext_refs_res = parse_interrupts_extended(node);
        if (ext_refs_res.has_value()) {
            return resolve_interrupt_refs_to_specs(ext_refs_res.value());
        }
        if (ext_refs_res.error() != ErrCode::ENTRY_NOT_FOUND) {
            propagate_return(ext_refs_res);
        }

        auto refs_res = parse_interrupts(node);
        propagate(refs_res);
        return resolve_interrupt_refs_to_specs(refs_res.value());
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
        const FDTDeviceNode &node) const noexcept {
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
            if (node_is_simple_bus(probe)) {
                scan_visible_nodes(*child, std::forward<Fn>(handler));
                continue;
            }
            handler(*child);
        }
    }

    void FDTProvider::register_memory_regions(device::DeviceModel &model) const {
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

#if defined(__ARCH_riscv64__)
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

        auto freq = units::frequency::from_hz(timebase_freq);
        model.set_platform(util::owner<device::Platform *>(
            new riscv::Riscv64Platform(freq)));
        loggers::DEVICE::INFO("已创建 Riscv64Platform, timebase-frequency=%lluHz",
                              static_cast<unsigned long long>(freq.to_hz()));
#elif defined(__ARCH_loongarch64__)
        auto freq = units::frequency::from_hz(LOONGARCH_TIMER_FREQ_HZ);
        model.set_platform(util::owner<device::Platform *>(
            new la64::LoongArch64Platform(freq)));
        loggers::DEVICE::INFO(
            "已创建 LoongArch64Platform, timer-frequency=%lluHz",
            static_cast<unsigned long long>(freq.to_hz()));
#endif
    }

    void FDTProvider::register_cpus(device::DeviceModel &model) const {
        auto &cpus = model.cpus();
        _irq_domains.clear();
        _cpu_intc_candidates.clear();
        _local_intc_map.clear();

        loggers::DEVICE::DEBUG("开始更新 CPU 组信息");
        cpus.cleanup();
        cpus.topology.cleanup();

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
        std::unordered_map<phandle_t, device::cpuid_t> cpu_phandle_map;
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

        std::sort(parsed_cpus.begin(), parsed_cpus.end(),
                  [](const ParsedCpu &lhs, const ParsedCpu &rhs) {
                      return lhs.id < rhs.id;
                  });

#if defined(__ARCH_riscv64__)
        for (const auto &parsed : parsed_cpus) {
            Node *intc_node = _config.get_node_by_phandle(parsed.cpu_intc_phandle);
            if (intc_node == nullptr) {
                loggers::DEVICE::ERROR(
                    "CPU %u 的本地中断节点不存在: phandle=%u", parsed.id,
                    parsed.cpu_intc_phandle);
                return;
            }
            _cpu_intc_candidates.push_back(CpuIntcDescriptor{
                .node       = intc_node,
                .hart_id    = parsed.id,
                .identifier = static_cast<intc_t>(parsed.cpu_intc_phandle),
                .name       = "riscv,cpu-intc",
            });
        }
#endif

        std::vector<device::cpuid_t> cpu_ids;
        cpu_ids.reserve(parsed_cpus.size());
        for (const auto &parsed : parsed_cpus) {
#if defined(__ARCH_riscv64__)
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
#elif defined(__ARCH_loongarch64__)
            auto cpu_res = device::LoongArch64Cpu::Builder()
                               .id(parsed.id)
                               .model("Loongson-3A5000")
                               .frequency(cpu_frequency)
                               .isa_string("")
                               .mmu_type("")
                               .caches({device::CacheInfo{
                                   .level     = device::CacheInfo::Level::EMPTY,
                                   .size      = 0,
                                   .line_size = 0,
                               }})
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
#endif
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
        device::ICInitGraph graph{};
        auto *driver_model = &driver::DriverModel::inst();

        for (size_t node_id = 0; node_id < model.device_nodes().size(); ++node_id) {
            auto *base_node = model.device_nodes()[node_id].get();
            if (base_node == nullptr) {
                continue;
            }
            auto *fdt_node = static_cast<const FDTDeviceNode *>(base_node);
            auto *irq_factory = find_irq_factory_for_node(*driver_model,
                                                          *fdt_node);
            if (irq_factory == nullptr) {
                continue;
            }

            driver::domain_t id =
                static_cast<driver::domain_t>(fdt_node->raw_node().phandle);
            auto add_res = graph.add_node(id, static_cast<int>(node_id),
                                          const_cast<FDTDeviceNode *>(fdt_node));
            if (!add_res.has_value()) {
                loggers::DEVICE::FATAL("构建中断控制器图失败: node=%s err=%s",
                                       fdt_node->raw_node().name.c_str(),
                                       to_cstring(add_res.error()));
                panic("构建中断控制器图失败");
            }
        }

        for (const auto &node_owner : model.device_nodes()) {
            auto *base_node = node_owner.get();
            if (base_node == nullptr) {
                continue;
            }
            auto *fdt_node = static_cast<const FDTDeviceNode *>(base_node);
            auto *irq_factory = find_irq_factory_for_node(*driver_model,
                                                          *fdt_node);
            if (irq_factory == nullptr) {
                continue;
            }
            auto parents_res =
                interrupt_parent_phandles_for_node(*this, fdt_node->raw_node());
            if (!parents_res.has_value()) {
                loggers::DEVICE::FATAL(
                    "解析中断控制器依赖关系失败: node=%s err=%s",
                    fdt_node->raw_node().name.c_str(),
                    to_cstring(parents_res.error()));
                panic("解析中断控制器依赖关系失败");
            }

            driver::domain_t child =
                static_cast<driver::domain_t>(fdt_node->raw_node().phandle);
            for (auto parent_phandle : parents_res.value()) {
                driver::domain_t parent =
                    static_cast<driver::domain_t>(parent_phandle);
                auto edge_res = graph.add_edge(parent, child);
                if (!edge_res.has_value() &&
                    edge_res.error() != ErrCode::ENTRY_NOT_FOUND)
                {
                    loggers::DEVICE::FATAL(
                        "构建中断控制器依赖边失败: parent=%u child=%u err=%s",
                        parent, child, to_cstring(edge_res.error()));
                    panic("构建中断控制器依赖边失败");
                }
            }
        }

        auto ordered_res = graph.topo_sort();
        if (!ordered_res.has_value()) {
            loggers::DEVICE::FATAL("中断控制器拓扑排序失败: err=%s",
                                   to_cstring(ordered_res.error()));
            panic("中断控制器拓扑排序失败");
        }
        auto ordered = std::move(ordered_res.value());

        for (auto *device_node : ordered) {
            if (device_node == nullptr) {
                continue;
            }
            auto *fdt_node = static_cast<FDTDeviceNode *>(device_node);
            auto *irq_factory = find_irq_factory_for_node(*driver_model,
                                                          *fdt_node);
            if (irq_factory == nullptr) {
                continue;
            }

            loggers::DEVICE::DEBUG("按拓扑序初始化 IRQ 控制器: node=%s",
                                   fdt_node->raw_node().name.c_str());
            auto device_res = driver_model->create_irq_driver(fdt_node);
            if (!device_res.has_value()) {
                loggers::DEVICE::ERROR("注册 IRQ 控制器失败: node=%s err=%s",
                                       fdt_node->raw_node().name.c_str(),
                                       to_cstring(device_res.error()));
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

    Result<void> FDTProvider::register_device(
        device::DeviceModel &model) const {
        register_memory_regions(model);
        register_platform(model);
        register_cpus(model);
        register_nodes(model);
        register_intcs(model);
        register_clock_virq(model);
        void_return();
    }
}  // namespace fdt
