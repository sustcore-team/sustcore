/**
 * @file cpu.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CPU device model implementation
 * @version alpha-1.0.0
 * @date 2026-05-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/device/platform.h>
#include <device/cpu.h>
#include <device/model.h>
#include <logger.h>
#include <sbi/sbi.h>
#include <sbi/sbi_enum.h>

#include <algorithm>
#include <functional>
#include <ranges>

namespace {
    /**
     * @brief 判断 CPU 是否属于给定拓扑节点.
     */
    [[nodiscard]]
    bool cpu_in_node(const device::CpuTopoNode &node,
                     device::cpuid_t cpu_id) noexcept {
        return std::ranges::find(node.cpu_ids, cpu_id) != node.cpu_ids.end();
    }

    /**
     * @brief 在拓扑树中按节点 ID 查找只读节点.
     */
    [[nodiscard]]
    const device::CpuTopoNode *find_node_by_id(const device::CpuTopoNode *node,
                                               device::topo_t id) noexcept {
        if (node == nullptr) {
            return nullptr;
        }
        if (node->id == id) {
            return node;
        }

        for (const auto &child : node->children) {
            if (auto *match = find_node_by_id(child.get(), id);
                match != nullptr)
            {
                return match;
            }
        }
        return nullptr;
    }

    /**
     * @brief 在拓扑树中按节点 ID 查找可写节点.
     */
    [[nodiscard]]
    device::CpuTopoNode *find_node_by_id(device::CpuTopoNode *node,
                                         device::topo_t id) noexcept {
        return const_cast<device::CpuTopoNode *>(
            find_node_by_id(const_cast<const device::CpuTopoNode *>(node), id));
    }

    /**
     * @brief 构造从根到目标 CPU 的拓扑路径.
     */
    [[nodiscard]]
    bool find_cpu_path(
        const device::CpuTopoNode *node, device::cpuid_t cpu_id,
        std::vector<const device::CpuTopoNode *> &path) noexcept {
        if (node == nullptr || !cpu_in_node(*node, cpu_id)) {
            return false;
        }

        path.push_back(node);
        for (const auto &child : node->children) {
            if (find_cpu_path(child.get(), cpu_id, path)) {
                return true;
            }
        }
        return true;
    }

    /**
     * @brief 自底向上聚合节点覆盖的 CPU 列表.
     */
    void aggregate_cpu_ids(device::CpuTopoNode &node) noexcept {
        std::vector<device::cpuid_t> aggregated = node.cpu_ids;
        for (auto &child : node.children) {
            aggregate_cpu_ids(*child);
            aggregated.insert(aggregated.end(), child->cpu_ids.begin(),
                              child->cpu_ids.end());
        }
        std::ranges::sort(aggregated);
        aggregated.erase(
            std::ranges::unique(aggregated,
                                [](device::cpuid_t lhs, device::cpuid_t rhs) {
                                    return lhs == rhs;
                                }),
            aggregated.end());
        node.cpu_ids = std::move(aggregated);
    }

    /**
     * @brief 将拓扑层级转换为可读字符串.
     */
    [[nodiscard]]
    const char *to_cstring(device::CpuTopoLevel level) noexcept {
        switch (level) {
            case device::CpuTopoLevel::THREAD:  return "THREAD";
            case device::CpuTopoLevel::CORE:    return "CORE";
            case device::CpuTopoLevel::CLUSTER: return "CLUSTER";
            case device::CpuTopoLevel::PACKAGE: return "PACKAGE";
            case device::CpuTopoLevel::NUMA:    return "NUMA";
            default:                            return "UNKNOWN";
        }
    }

    /**
     * @brief 递归打印 CPU 拓扑节点.
     */
    void print_topology_node(const device::CpuTopoNode &node,
                             size_t depth) noexcept {
        char indent[32]     = {};
        size_t indent_width = depth * 2;
        if (indent_width >= sizeof(indent)) {
            indent_width = sizeof(indent) - 1;
        }
        for (size_t i = 0; i < indent_width; ++i) {
            indent[i] = ' ';
        }

        loggers::SUSTCORE::INFO("%s拓扑节点 level=%s id=%u cpu_count=%u",
                                indent, to_cstring(node.level), node.id,
                                static_cast<unsigned>(node.cpu_ids.size()));
        for (device::cpuid_t cpu_id : node.cpu_ids) {
            loggers::SUSTCORE::INFO("%s  cpu=%u", indent, cpu_id);
        }

        for (const auto &child : node.children) {
            print_topology_node(*child, depth + 1);
        }
    }
}  // namespace

namespace device {
    /**
     * @brief 构造一个 RISC-V CPU 对象.
     */
    RiscV64Cpu::RiscV64Cpu(cpuid_t id, std::string model, units::frequency freq,
                           std::string isa_string, std::string mmu_type,
                           std::vector<CacheInfo> caches,
                           driver::intc_t local_intc) noexcept
        : _id(id),
          _model(std::move(model)),
          _frequency(freq),
          _isa_string(std::move(isa_string)),
          _mmu_type(std::move(mmu_type)),
          _caches(std::move(caches)),
          _local_intc(local_intc) {}

    RiscV64Cpu::~RiscV64Cpu() = default;

    /**
     * @brief 获取 CPU ID.
     */
    cpuid_t RiscV64Cpu::id() const noexcept {
        return _id;
    }

    /**
     * @brief 获取 CPU 型号.
     */
    std::string RiscV64Cpu::model() const {
        return _model;
    }

    /**
     * @brief 获取 CPU 频率.
     */
    units::frequency RiscV64Cpu::frequency() const noexcept {
        return _frequency;
    }

    /**
     * @brief 获取缓存信息列表.
     */
    std::vector<CacheInfo> RiscV64Cpu::caches() const {
        return _caches;
    }

    /**
     * @brief 获取 MMU 类型字符串.
     */
    std::string RiscV64Cpu::mmu_type() const {
        return _mmu_type;
    }

    /**
     * @brief 发送 IPI.
     */
    Result<void> RiscV64Cpu::send_ipi() {
        unexpect_return(ErrCode::NOT_SUPPORTED);
        // loggers::INTERRUPT::DEBUG("向 hart %u 发送 SBI IPI", _id);

        // if (_id >= sizeof(umb_t) * 8U) {
        //     loggers::INTERRUPT::ERROR("hart %u 超出当前 SBI mask 宽度", _id);
        //     unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        // }

        // const umb_t hart_mask = static_cast<umb_t>(1ULL << _id);
        // SBIRet ret            = sbi_send_ipi(hart_mask, 0);
        // if (ret.error != SBI_SUCCESS) {
        //     ErrCode err = map_sbi_error(ret.error);
        //     loggers::INTERRUPT::ERROR(
        //         "向 hart %u 发送 SBI IPI 失败: sbi=%ld err=%s", _id,
        //         static_cast<long>(ret.error), to_cstring(err));
        //     unexpect_return(err);
        // }

        // void_return();
    }

    /**
     * @brief 获取 ISA 描述字符串.
     */
    std::string RiscV64Cpu::isa_string() const {
        return _isa_string;
    }

    /**
     * @brief 获取 CPU 本地中断端点标识.
     */
    driver::intc_t RiscV64Cpu::local_intc() const noexcept {
        return _local_intc;
    }

    /**
     * @brief 设置 CPU ID.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::id(cpuid_t cpu_id) noexcept {
        _id = cpu_id;
        return *this;
    }

    /**
     * @brief 设置 CPU 型号.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::model(std::string model) {
        _model = std::move(model);
        return *this;
    }

    /**
     * @brief 设置 CPU 频率.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::frequency(
        units::frequency frequency) noexcept {
        _frequency = frequency;
        return *this;
    }

    /**
     * @brief 设置 ISA 描述字符串.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::isa_string(
        std::string isa_string) {
        _isa_string = std::move(isa_string);
        return *this;
    }

    /**
     * @brief 设置 MMU 类型字符串.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::mmu_type(std::string mmu_type) {
        _mmu_type = std::move(mmu_type);
        return *this;
    }

    /**
     * @brief 设置缓存信息列表.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::caches(
        std::vector<CacheInfo> caches) {
        _caches = std::move(caches);
        return *this;
    }

    /**
     * @brief 设置 CPU 本地中断端点标识.
     */
    RiscV64Cpu::Builder &RiscV64Cpu::Builder::local_intc(
        driver::intc_t local_intc) noexcept {
        _local_intc = local_intc;
        return *this;
    }

    /**
     * @brief 构建 RiscV64Cpu 对象.
     */
    Result<util::owner<RiscV64Cpu *>> RiscV64Cpu::Builder::build() const {
        // 校验关键字段.
        if (!_id.has_value() || !_model.has_value() ||
            !_frequency.has_value() || !_isa_string.has_value() ||
            !_mmu_type.has_value() || !_local_intc.has_value())
        {
            loggers::DEVICE::ERROR("构建 RiscV64Cpu 失败: 关键字段缺失");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (_model->empty() || _isa_string->empty()) {
            loggers::DEVICE::ERROR(
                "构建 RiscV64Cpu 失败: model/isa_string 为空");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        // 构造最终对象.
        return util::owner(new RiscV64Cpu(*_id, *_model, *_frequency,
                                          *_isa_string, *_mmu_type, _caches,
                                          *_local_intc));
    }

    LoongArch64Cpu::LoongArch64Cpu(cpuid_t id, std::string model,
                                   units::frequency freq,
                                   std::string isa_string,
                                   std::string mmu_type,
                                   std::vector<CacheInfo> caches,
                                   driver::intc_t local_intc) noexcept
        : _id(id),
          _model(std::move(model)),
          _frequency(freq),
          _isa_string(std::move(isa_string)),
          _mmu_type(std::move(mmu_type)),
          _caches(std::move(caches)),
          _local_intc(local_intc) {}

    LoongArch64Cpu::~LoongArch64Cpu() = default;

    cpuid_t LoongArch64Cpu::id() const noexcept {
        return _id;
    }

    std::string LoongArch64Cpu::model() const {
        return _model;
    }

    units::frequency LoongArch64Cpu::frequency() const noexcept {
        return _frequency;
    }

    std::vector<CacheInfo> LoongArch64Cpu::caches() const {
        return _caches;
    }

    std::string LoongArch64Cpu::mmu_type() const {
        return _mmu_type;
    }

    std::string LoongArch64Cpu::isa_string() const {
        return _isa_string;
    }

    driver::intc_t LoongArch64Cpu::local_intc() const noexcept {
        if (!device::DeviceModel::initialized()) {
            return _local_intc;
        }
        auto *platform = device::DeviceModel::inst().platform();
        if (platform == nullptr || !platform->is<la64::LoongArch64Platform>()) {
            return _local_intc;
        }
        return platform->as<la64::LoongArch64Platform>()->global_intc();
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::id(
        cpuid_t cpu_id) noexcept {
        _id = cpu_id;
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::model(
        std::string model) {
        _model = std::move(model);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::frequency(
        units::frequency frequency) noexcept {
        _frequency = frequency;
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::isa_string(
        std::string isa_string) {
        _isa_string = std::move(isa_string);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::mmu_type(
        std::string mmu_type) {
        _mmu_type = std::move(mmu_type);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::caches(
        std::vector<CacheInfo> caches) {
        _caches = std::move(caches);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::local_intc(
        driver::intc_t local_intc) noexcept {
        _local_intc = local_intc;
        return *this;
    }

    Result<util::owner<LoongArch64Cpu *>> LoongArch64Cpu::Builder::build() const {
        if (!_id.has_value() || !_model.has_value() || !_frequency.has_value() ||
            !_isa_string.has_value() || !_mmu_type.has_value() ||
            !_local_intc.has_value())
        {
            loggers::DEVICE::ERROR("构建 LoongArch64Cpu 失败: 关键字段缺失");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (_model->empty()) {
            loggers::DEVICE::ERROR("构建 LoongArch64Cpu 失败: model 为空");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return util::owner(new LoongArch64Cpu(*_id, *_model, *_frequency,
                                              *_isa_string, *_mmu_type,
                                              _caches, *_local_intc));
    }

    /**
     * @brief 查询指定 CPU 在某层级的祖先节点.
     */
    Result<const CpuTopoNode *> CpuTopology::ancestor(CpuTopoLevel level,
                                                      cpuid_t cpu_id) const {
        if (!_root) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        std::vector<const CpuTopoNode *> path;
        if (!find_cpu_path(_root.get(), cpu_id, path)) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto it = std::ranges::find_if(path, [level](const CpuTopoNode *node) {
            return node->level == level;
        });
        if (it == path.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return *it;
    }

    /**
     * @brief 计算两个 CPU 共享的最高层级.
     */
    CpuTopoLevel CpuTopology::shared_level(cpuid_t cpu_id1,
                                           cpuid_t cpu_id2) const {
        if (!_root) {
            return CpuTopoLevel::THREAD;
        }

        std::vector<const CpuTopoNode *> path1;
        std::vector<const CpuTopoNode *> path2;
        if (!find_cpu_path(_root.get(), cpu_id1, path1) ||
            !find_cpu_path(_root.get(), cpu_id2, path2))
        {
            return CpuTopoLevel::THREAD;
        }

        CpuTopoLevel shared = CpuTopoLevel::THREAD;
        size_t limit        = std::ranges::min(path1.size(), path2.size());
        for (size_t i = 0; i < limit; ++i) {
            if (path1[i] != path2[i]) {
                break;
            }
            shared = path1[i]->level;
        }
        return shared;
    }

    /**
     * @brief 获取指定层级下与目标 CPU 同级的其他 CPU.
     */
    std::vector<cpuid_t> CpuTopology::siblings(cpuid_t cpu_id,
                                               CpuTopoLevel level) const {
        auto node_res = ancestor(level, cpu_id);
        if (!node_res.has_value()) {
            return {};
        }

        std::vector<cpuid_t> result = node_res.value()->cpu_ids;
        result.erase(std::ranges::remove(result, cpu_id), result.end());
        return result;
    }

    /**
     * @brief 打印整个 CPU 拓扑树.
     */
    void CpuTopology::print() const noexcept {
        if (!_root) {
            loggers::SUSTCORE::WARN("CPU 拓扑为空");
            return;
        }

        loggers::SUSTCORE::INFO("CPU 拓扑结构:");
        print_topology_node(*_root, 0);
    }

    /**
     * @brief 创建拓扑树根节点.
     */
    CpuTopologyBuilder &CpuTopologyBuilder::root(CpuTopoLevel level,
                                                 topo_t id) noexcept {
        cleanup();
        auto *node  = new CpuTopoNode();
        node->level = level;
        node->id    = id;
        _root       = util::owner<CpuTopoNode *>(node);
        return *this;
    }

    /**
     * @brief 向父节点追加子节点.
     */
    Result<CpuTopologyBuilder &> CpuTopologyBuilder::add_child(
        topo_t parent_id, CpuTopoLevel level, topo_t id) noexcept {
        if (!_root) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        CpuTopoNode *parent = find_node_by_id(_root.get(), parent_id);
        if (parent == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (find_node_by_id(_root.get(), id) != nullptr) {
            unexpect_return(ErrCode::KEY_DUPLICATED);
        }

        auto *child  = new CpuTopoNode();
        child->level = level;
        child->id    = id;
        parent->children.push_back(util::owner<CpuTopoNode *>(child));
        return std::ref(*this);
    }

    /**
     * @brief 为节点设置 CPU 列表.
     */
    Result<CpuTopologyBuilder &> CpuTopologyBuilder::cpus(
        topo_t node_id, std::vector<cpuid_t> cpu_ids) noexcept {
        if (!_root) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        CpuTopoNode *node = find_node_by_id(_root.get(), node_id);
        if (node == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        std::ranges::sort(cpu_ids);
        cpu_ids.erase(std::ranges::unique(cpu_ids), cpu_ids.end());
        node->cpu_ids = std::move(cpu_ids);
        return std::ref(*this);
    }

    /**
     * @brief 完成拓扑树构建.
     */
    Result<CpuTopology> CpuTopologyBuilder::build() noexcept {
        if (!_root) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        // 聚合整棵树上的 CPU 信息.
        aggregate_cpu_ids(*_root);

        // 交出根节点所有权.
        CpuTopology topology(util::owner<CpuTopoNode *>(_root.get()));
        _root = util::owner<CpuTopoNode *>(nullptr);
        return topology;
    }
}  // namespace device
