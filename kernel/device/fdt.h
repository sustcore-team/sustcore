/**
 * @file fdt.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2026-05-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/factory.h>
#include <driver/serial.h>
#include <device/model.h>
#include <logger.h>
#include <sus/rtti.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/boot.h>
#include <sustcore/errcode.h>

#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fdt {
    using driver::domain_t;
    using driver::DriverBase;
    using driver::hwirq_t;
    using driver::intc_t;
    using driver::virq_t;

    namespace detail {
        [[nodiscard]]
        constexpr bool parse_be_integer(const char *data, size_t size,
                                        uint64_t &value) {
            if (data == nullptr || size == 0 || size > sizeof(uint64_t)) {
                return false;
            }

            value = 0;
            for (size_t i = 0; i < size; ++i) {
                value = (value << 8) | static_cast<unsigned char>(data[i]);
            }
            return true;
        }
    }  // namespace detail

    using phandle_t = b32;

    constexpr const char *COMPATIBLE_PROP  = "compatible";
    constexpr const char *DEVICE_TYPE_PROP = "device_type";
    constexpr const char *STATUS_PROP      = "status";
    constexpr const char *OKAY_STATUS      = "okay";
    constexpr const char *OK_STATUS        = "ok";
    constexpr const char *DISABLED_STATUS  = "disabled";

    struct RegionCells {
        size_t addr_cells;
        size_t size_cells;
    };

    using LocalInterruptTargetMap =
        std::unordered_map<phandle_t, device::cpuid_t>;

    struct CpuIntcDescriptor {
        const struct Node *node = nullptr;
        device::cpuid_t hart_id = 0;
        intc_t identifier = driver::INVALID_ICTRL_ID;
        std::string name;
    };

    struct Property {
        std::string name;
        const char *data;  // 直接指向 dtb 中的数据
        size_t size;       // 数据大小 (字节)

        [[nodiscard]]
        constexpr std::vector<std::string> as_string_list() const {
            std::vector<std::string> result;
            std::string str = "";
            for (int i = 0; i < size; i++) {
                if (data[i] == '\0') {
                    if (!str.empty()) {
                        result.push_back(str);
                        str.clear();
                    }
                } else {
                    str += data[i];
                }
            }
            if (!str.empty()) {
                result.push_back(str);
            }
            return result;
        }

        /**
         * @brief 将属性解析为设备树 reg 风格的地址区间列表.
         *
         * 属性数据会按大端 cell 序列解释, 每个 cell 固定为 4 字节.
         * 每个区间由 `addr_cells` 个地址 cell 与 `size_cells` 个长度 cell
         * 组成, 返回值中的每个 `PhyArea` 均满足 `[begin, begin + size)`.
         *
         * 若属性长度非法、cell 数为 0、或单个地址/长度超出 64 位表示范围,
         * 则在调试构建中触发 `assert`, 并返回空列表作为哨兵值.
         *
         * @param cells 地址与长度字段各自占用的 cell 数量.
         * @return std::vector<PhyArea> 解析得到的区间列表.
         */
        [[nodiscard]]
        constexpr std::vector<PhyArea> as_regions(
            const RegionCells &cells) const {
            constexpr size_t CELL_SIZE = sizeof(sus_u32);

            if (data == nullptr || cells.addr_cells == 0 ||
                cells.size_cells == 0)
            {
                assert(false && "invalid property buffer or region cells");
                return {};
            }

            size_t total_cells      = cells.addr_cells + cells.size_cells;
            size_t bytes_per_region = total_cells * CELL_SIZE;
            size_t addr_size        = cells.addr_cells * CELL_SIZE;
            size_t size_size        = cells.size_cells * CELL_SIZE;

            if (bytes_per_region == 0 || size % bytes_per_region != 0) {
                assert(false && "invalid property size for reg regions");
                return {};
            }
            if (addr_size > sizeof(uint64_t) || size_size > sizeof(uint64_t)) {
                assert(false && "reg cell width exceeds uint64_t");
                return {};
            }

            std::vector<PhyArea> result;
            result.reserve(size / bytes_per_region);
            for (size_t offset = 0; offset < size; offset += bytes_per_region) {
                uint64_t begin_raw = 0;
                uint64_t size_raw  = 0;
                bool ok = detail::parse_be_integer(data + offset, addr_size,
                                                   begin_raw) &&
                          detail::parse_be_integer(data + offset + addr_size,
                                                   size_size, size_raw);
                if (!ok ||
                    size_raw > static_cast<uint64_t>(MAX_ADDR) - begin_raw)
                {
                    assert(false && "invalid reg region value");
                    return {};
                }

                result.push_back(PhyArea(
                    PhyAddr(static_cast<addr_t>(begin_raw)),
                    PhyAddr(static_cast<addr_t>(begin_raw + size_raw))));
            }
            return result;
        }

        /**
         * @brief 将属性解析为大端无符号整数.
         *
         * 属性数据按 DTB 的大端编码解释, 支持 1 到 8 字节的整数字段.
         * 若属性为空、数据指针为空、或长度超过 64 位表示范围, 则在调试构建
         * 中触发 `assert`, 并返回 `0` 作为哨兵值.
         *
         * @return uint64_t 解析出的无符号整数值.
         */
        [[nodiscard]]
        constexpr uint64_t as_integral() const {
            uint64_t value = 0;
            if (!detail::parse_be_integer(data, size, value)) {
                assert(false && "invalid property integral width");
                return 0;
            }
            return value;
        }
        [[nodiscard]]
        constexpr phandle_t as_phandle() const {
            return as_integral();
        }
        [[nodiscard]]
        constexpr std::string as_string() const {
            return as_string_list().empty() ? "" : as_string_list()[0];
        }
        [[nodiscard]]
        constexpr std::vector<byte> as_byte_array() const {
            return {data, data + size};
        }
    };

    struct Node {
        std::string name;
        std::unordered_map<std::string, util::owner<Property *>> properties;
        std::unordered_map<std::string, util::owner<Node *>> children;
        Node *parent      = nullptr;
        phandle_t phandle = 0;

        Node() = default;

        Node(const Node &other)
            : name(other.name),
              properties(),
              children(),
              parent(other.parent),
              phandle(other.phandle) {
            for (const auto &[key, prop] : other.properties) {
                properties[key] = util::owner(new Property(*prop));
            }
            for (const auto &[key, child] : other.children) {
                children[key] = util::owner(new Node(*child));
            }
        }
        Node &operator=(const Node &other) {
            if (this != &other) {
                cleanup();

                name    = other.name;
                parent  = other.parent;
                phandle = other.phandle;
                for (const auto &[key, prop] : other.properties) {
                    properties[key] = util::owner(new Property(*prop));
                }
                for (const auto &[key, child] : other.children) {
                    children[key] = util::owner(new Node(*child));
                }
            }
            return *this;
        }
        Node(Node &&other)
            : name(std::move(other.name)),
              properties(std::move(other.properties)),
              children(std::move(other.children)),
              parent(other.parent),
              phandle(other.phandle) {}
        Node &operator=(Node &&other) {
            if (this != &other) {
                cleanup();
                name       = std::move(other.name);
                properties = std::move(other.properties);
                children   = std::move(other.children);
                parent     = other.parent;
                phandle    = other.phandle;
            }
            return *this;
        }

        void cleanup() {
            for (auto &[_, prop] : properties) {
                delete prop;
            }
            for (auto &[_, child] : children) {
                delete child;
            }
            properties.clear();
            children.clear();
        }

        ~Node() {
            cleanup();
        }
    };

    struct Configuration {
        util::owner<Node *> root;
        std::unordered_map<phandle_t, Node *> phandle_map;

        [[nodiscard]]
        Node *get_node_by_path(const std::string &path) const;
        [[nodiscard]]
        Node *get_node_by_phandle(phandle_t phandle) const;
        [[nodiscard]]
        Result<void> add_node(Node *parent, util::owner<Node *> new_node);
        [[nodiscard]]
        Result<void> remove_node(Node *node);
    };

    void make_config(void *dtb, Configuration &config);

    class FDTProvider;

    /**
     * @brief 将 FDT 原始节点暴露为统一设备语义接口.
     */
    class FDTDeviceNode final : public device::DeviceNode {
    public:
        /**
         * @brief 使用 FDT 上下文构造统一设备节点包装器.
         *
         * @param provider 所属 FDT provider.
         * @param config FDT 配置树.
         * @param node 目标原始节点.
         */
        FDTDeviceNode(const FDTProvider &provider, const Configuration &config,
                      const Node &node) noexcept;

        /**
         * @brief 销毁节点包装器.
         */
        ~FDTDeviceNode() override = default;

        /**
         * @brief 获取统一设备节点名称.
         *
         * @return const char* FDT 节点名称字符串.
         */
        [[nodiscard]]
        const char *name() const noexcept override;

        /**
         * @brief 获取节点所属平台名称.
         *
         * @return const char* 固定返回 "fdt".
         */
        [[nodiscard]]
        const char *platform() const noexcept override;

        /**
         * @brief 查询统一语义下的节点属性.
         *
         * @param name 统一属性名.
         * @return Optional<device::DevicePropView> 查询结果.
         */
        [[nodiscard]]
        Optional<device::DevicePropView> property(
            const std::string_view &name) const override;

        /**
         * @brief 获取对应的原始 FDT 节点只读引用.
         *
         * @return const Node& 原始节点引用.
         */
        [[nodiscard]]
        const Node &raw_node() const noexcept {
            return *_node;
        }

    private:
        /**
         * @brief 读取原始 FDT 属性并包装为统一属性视图.
         *
         * @param prop_name FDT 原始属性名.
         * @return Optional<device::DevicePropView> 查询结果.
         */
        [[nodiscard]]
        Optional<device::DevicePropView> raw_property(
            const char *prop_name) const noexcept;

        /**
         * @brief 解析当前节点的 MMIO 区域列表.
         *
         * @return Optional<device::DevicePropView> 结构化 MMIO 结果.
         */
        [[nodiscard]]
        Optional<device::DevicePropView> mmio_property() const noexcept;

        /**
         * @brief 解析当前节点的统一 IRQ 列表.
         *
         * @return Optional<device::DevicePropView> 结构化 IRQ 结果.
         */
        [[nodiscard]]
        Optional<device::DevicePropView> irq_property() const noexcept;

        /**
         * @brief 解析当前节点的中断父节点标识.
         *
         * @return Optional<device::DevicePropView> 中断父节点结果.
         */
        [[nodiscard]]
        Optional<device::DevicePropView> interrupt_parent_property()
            const noexcept;

        const FDTProvider *_provider = nullptr;
        const Configuration *_config = nullptr;
        const Node *_node            = nullptr;
    };

    class FDTProvider : public device::DeviceProvider {
        friend class FDTDeviceNode;

    private:
        using InterruptRef = std::pair<phandle_t, hwirq_t>;

        Configuration _config;
        mutable std::unordered_map<phandle_t, domain_t> _irq_domains;
        mutable LocalInterruptTargetMap _local_intc_map;
        mutable std::vector<CpuIntcDescriptor> _cpu_intc_candidates;

        /**
         * @brief 在 FDT 解析器内部登记 phandle 到中断域的映射.
         *
         * @param phandle 中断控制器节点的 phandle.
         * @param domain 已注册的中断域对象.
         * @return Result<void> 登记结果.
         */
        [[nodiscard]]
        Result<void> register_irq_domain(phandle_t phandle,
                                         const driver::IrqDomain &domain) const;

        /**
         * @brief 通过 phandle 解析对应的中断域.
         *
         * @param phandle 中断控制器节点的 phandle.
         * @param irqman 全局中断管理器.
         * @return Result<driver::IrqDomain&> 对应的中断域引用.
         */
        [[nodiscard]]
        Result<driver::IrqDomain &> resolve_irq_domain(
            phandle_t phandle, driver::IrqManager &irqman) const;

        /**
         * @brief 解析中断控制器的 #interrupt-cells 配置.
         *
         * @param controller_phandle 中断控制器节点的 phandle.
         * @return Result<size_t> 中断描述占用的 cell 数.
         */
        [[nodiscard]]
        Result<size_t> interrupt_cells_for_controller(
            phandle_t controller_phandle) const;

        /**
         * @brief 沿父节点链查找节点生效的 interrupt-parent.
         *
         * @param node 待查询节点.
         * @return Result<phandle_t> 继承解析后的 interrupt-parent phandle.
         */
        [[nodiscard]]
        Result<phandle_t> resolve_interrupt_parent(const Node &node) const;

        /**
         * @brief 将 interrupts-extended 属性解析为中断引用列表.
         *
         * 返回值顺序与属性中的条目顺序一致. 当前仅支持
         * `#interrupt-cells == 1` 的中断控制器编码.
         *
         * @param node 待解析节点.
         * @return Result<std::vector<InterruptRef>> 解析得到的中断引用列表.
         */
        [[nodiscard]]
        Result<std::vector<InterruptRef>> parse_interrupts_extended(
            const Node &node) const;

        /**
         * @brief 将 interrupt-parent + interrupts 解析为中断引用列表.
         *
         * 返回值顺序与属性中的条目顺序一致. 当前仅支持
         * `#interrupt-cells == 1` 的中断控制器编码.
         *
         * @param node 待解析节点.
         * @return Result<std::vector<InterruptRef>> 解析得到的中断引用列表.
         */
        [[nodiscard]]
        Result<std::vector<InterruptRef>> parse_interrupts(
            const Node &node) const;

        /**
         * @brief 将一组中断引用解析为稳定的 virq 列表.
         *
         * 解析时会根据 FDT 内部维护的 phandle -> IrqDomain 映射查找目标域,
         * 再由 IrqManager 为域内 hwirq 分配或复用稳定 virq.
         *
         * @param refs 待解析的中断引用列表.
         * @param irqman 全局中断管理器.
         * @return Result<std::vector<virq_t>> 对应的 virq 列表.
         */
        [[nodiscard]]
        Result<std::vector<virq_t>> resolve_interrupt_refs_to_virqs(
            const std::vector<InterruptRef> &refs,
            driver::IrqManager &irqman) const;

        /**
         * @brief 优先按 interrupts-extended, 否则按 interrupt-parent +
         * interrupts 解析节点中断.
         *
         * 返回值顺序与设备树属性中的中断声明顺序一致.
         *
         * @param node 待解析节点.
         * @param irqman 全局中断管理器.
         * @return Result<std::vector<virq_t>> 解析得到的 virq 列表.
         */
        [[nodiscard]]
        Result<std::vector<virq_t>> parse_interrupt_virqs(
            const Node &node, driver::IrqManager &irqman) const;

        void append_as_regions(std::vector<MemRegion> &regions,
                               const RegionCells &cells, const Property &prop,
                               MemRegion::MemoryStatus status) const;

        bool is_memory_node(Node &node) const;
        [[nodiscard]]
        bool node_is_simple_bus(const device::DeviceNode &node) const noexcept;
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
        FDTProvider(void *dtb);
        /**
         * @brief 向普通设备工厂注册表登记默认 FDT 设备工厂.
         */
        void init_device_factories() noexcept;
        /**
         * @brief 向 IRQ 工厂注册表登记默认 FDT IRQ 工厂.
         */
        void init_irq_factories() noexcept;

        /**
         * @brief 公开解析 interrupts-extended 的只读入口.
         *
         * @param node 待解析节点.
         * @return Result<std::vector<InterruptRef>> 中断引用列表.
         */
        [[nodiscard]]
        Result<std::vector<InterruptRef>> parse_interrupts_extended_view(
            const Node &node) const {
            return parse_interrupts_extended(node);
        }

        /**
         * @brief 公开按 phandle 查找 IRQ domain 的只读入口.
         *
         * @param phandle 中断控制器 phandle.
         * @param irqman 全局中断管理器.
         * @return Result<driver::IrqDomain&> 对应中断域.
         */
        [[nodiscard]]
        Result<driver::IrqDomain &> resolve_irq_domain_view(
            phandle_t phandle, driver::IrqManager &irqman) const {
            return resolve_irq_domain(phandle, irqman);
        }

        /**
         * @brief 公开登记 IRQ domain 的只读入口.
         *
         * @param phandle 中断控制器 phandle.
         * @param domain 目标中断域.
         * @return Result<void> 登记结果.
         */
        [[nodiscard]]
        Result<void> register_irq_domain_view(
            phandle_t phandle, const driver::IrqDomain &domain) const {
            return register_irq_domain(phandle, domain);
        }

        /**
         * @brief 获取 FDT 配置树.
         *
         * @return const Configuration& FDT 配置树.
         */
        [[nodiscard]]
        const Configuration &config() const noexcept {
            return _config;
        }

        FDTProvider(void *dtb, int) = delete;
        FDTProvider(const FDTProvider &)            = delete;
        FDTProvider &operator=(const FDTProvider &) = delete;
        FDTProvider(FDTProvider &&)                 = delete;
        FDTProvider &operator=(FDTProvider &&)      = delete;
        void register_device(device::DeviceModel &model) const override;
        [[nodiscard]]
        const char *name() const override {
            return "fdt";
        }
    };
}  // namespace fdt
