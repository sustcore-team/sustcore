/**
 * @file cpu.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief cpu
 * @version alpha-1.0.0
 * @date 2026-05-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/int/base.h>
#include <sus/rtti.h>
#include <sus/units.h>

#include <optional>
#include <string>
#include <vector>

namespace device {
    /**
     * @brief CPU 电源状态.
     */
    enum class CpuPowerState { OFFLINE, ONLINE, HALTED, WFI };

    /**
     * @brief CPU 缓存信息.
     */
    struct CacheInfo {
        /**
         * @brief 缓存层级枚举.
         */
        enum class Level { L1I, L1D, L2, L3 };
        Level level;
        size_t size;       // 大小
        size_t line_size;  // 缓存行
    };

    /**
     * @brief CPU 架构类型.
     */
    enum class CpuType { UNKNOWN, RISCV64, X86_64, ARM64, LOONGARCH64 };

    /**
     * @brief 抽象 CPU 设备接口.
     */
    class Cpu : public RTTIBase<Cpu, CpuType> {
    public:
        virtual ~Cpu() = default;
        [[nodiscard]]
        virtual cpuid_t id() const = 0;
        [[nodiscard]]
        virtual std::string model() const = 0;
        [[nodiscard]]
        virtual units::frequency frequency() const = 0;
        [[nodiscard]]
        virtual CpuPowerState state() const = 0;
        [[nodiscard]]
        virtual std::vector<CacheInfo> caches() const = 0;
        [[nodiscard]]
        virtual std::string mmu_type() const = 0;
        [[nodiscard]]
        virtual Result<void> start() = 0;
        [[nodiscard]]
        virtual Result<void> stop() = 0;
        [[nodiscard]]
        virtual Result<void> send_ipi() = 0;
        [[nodiscard]]
        virtual driver::intc_t local_intc() const = 0;
    };

    /**
     * @brief RISC-V 64 CPU 设备模型.
     */
    class RiscV64Cpu : public Cpu {
    public:
        [[nodiscard]]
        CpuType type_id() const override {
            return CpuType::RISCV64;
        }

        /**
         * @brief 销毁 CPU 设备对象.
         */
        virtual ~RiscV64Cpu() override;
        /**
         * @brief 获取 CPU ID.
         */
        [[nodiscard]]
        cpuid_t id() const noexcept override;
        /**
         * @brief 获取 CPU 型号.
         */
        [[nodiscard]]
        std::string model() const override;
        /**
         * @brief 获取 CPU 频率.
         */
        [[nodiscard]]
        units::frequency frequency() const noexcept override;
        /**
         * @brief 获取 CPU 运行状态.
         */
        [[nodiscard]]
        CpuPowerState state() const noexcept override {
            return CpuPowerState::ONLINE;
        }
        /**
         * @brief 获取缓存信息列表.
         */
        [[nodiscard]]
        std::vector<CacheInfo> caches() const override;
        /**
         * @brief 获取 MMU 类型字符串.
         */
        [[nodiscard]]
        std::string mmu_type() const override;
        [[nodiscard]]
        Result<void> start() override {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        [[nodiscard]]
        Result<void> stop() override {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        [[nodiscard]]
        Result<void> send_ipi() override;
        /**
         * @brief 获取 ISA 描述字符串.
         */
        [[nodiscard]]
        std::string isa_string() const;
        /**
         * @brief 获取 CPU 本地中断端点标识.
         */
        [[nodiscard]]
        driver::intc_t local_intc() const noexcept override;

    private:
        cpuid_t _id;
        std::string _model;
        units::frequency _frequency;
        std::string _isa_string;
        std::string _mmu_type;
        std::vector<CacheInfo> _caches;
        driver::intc_t _local_intc;

        RiscV64Cpu(cpuid_t id, std::string model, units::frequency freq,
                   std::string isa_string, std::string mmu_type,
                   std::vector<CacheInfo> caches,
                   driver::intc_t local_intc) noexcept;

    public:
        /**
         * @brief RiscV64Cpu 链式构造器.
         */
        class Builder {
        private:
            std::optional<cpuid_t> _id;
            std::optional<std::string> _model;
            std::optional<units::frequency> _frequency;
            std::optional<std::string> _isa_string;
            std::optional<std::string> _mmu_type;
            std::vector<CacheInfo> _caches;
            std::optional<driver::intc_t> _local_intc;

        public:
            /**
             * @brief 设置 CPU ID.
             */
            Builder &id(cpuid_t cpu_id) noexcept;
            /**
             * @brief 设置 CPU 型号.
             */
            Builder &model(std::string model);
            /**
             * @brief 设置 CPU 频率.
             */
            Builder &frequency(units::frequency frequency) noexcept;
            /**
             * @brief 设置 ISA 字符串.
             */
            Builder &isa_string(std::string isa_string);
            /**
             * @brief 设置 MMU 类型字符串.
             */
            Builder &mmu_type(std::string mmu_type);
            /**
             * @brief 设置缓存信息列表.
             */
            Builder &caches(std::vector<CacheInfo> caches);
            /**
             * @brief 设置 CPU 本地中断端点标识.
             */
            Builder &local_intc(driver::intc_t local_intc) noexcept;
            /**
             * @brief 构建 CPU 对象.
             *
             * @return Result<util::owner<RiscV64Cpu *>> 构建结果.
             */
            [[nodiscard]]
            Result<util::owner<RiscV64Cpu *>> build() const;
        };
    };

    /**
     * @brief CPU 拓扑层级.
     */
    enum class CpuTopoLevel { THREAD, CORE, CLUSTER, PACKAGE, NUMA };

    /**
     * @brief CPU 拓扑节点.
     */
    struct CpuTopoNode {
        CpuTopoLevel level;
        topo_t id;
        // 该节点下包含的所有逻辑 Cpu
        std::vector<cpuid_t> cpu_ids;
        std::vector<util::owner<CpuTopoNode *>> children;

        /**
         * @brief 释放子节点.
         */
        void cleanup() {
            for (auto &child : children) {
                delete child.get();
            }
            children.clear();
        }

        CpuTopoNode() = default;

        CpuTopoNode(CpuTopoNode &)            = delete;
        CpuTopoNode &operator=(CpuTopoNode &) = delete;

        CpuTopoNode(CpuTopoNode &&other) noexcept
            : level(other.level),
              id(other.id),
              cpu_ids(std::move(other.cpu_ids)),
              children(std::move(other.children)) {}
        CpuTopoNode &operator=(CpuTopoNode &&other) noexcept {
            if (this != &other) {
                cleanup();
                level    = other.level;
                id       = other.id;
                cpu_ids  = std::move(other.cpu_ids);
                children = std::move(other.children);
            }
            return *this;
        }

        ~CpuTopoNode() {
            cleanup();
        }
    };

    /**
     * @brief CPU 拓扑树查询接口.
     */
    class CpuTopology {
    private:
        util::owner<CpuTopoNode *> _root;

    public:
        CpuTopology() noexcept : _root(nullptr) {}
        /**
         * @brief 使用根节点构造拓扑树.
         */
        explicit CpuTopology(util::owner<CpuTopoNode *> root) noexcept
            : _root(root.get()) {}

        CpuTopology(CpuTopology &)            = delete;
        CpuTopology &operator=(CpuTopology &) = delete;

        /**
         * @brief 释放整棵拓扑树.
         */
        void cleanup() {
            if (_root) {
                delete _root.get();
                _root = util::owner<CpuTopoNode *>(nullptr);
            }
        }

        CpuTopology(CpuTopology &&other) noexcept : _root(other._root.get()) {
            other._root = util::owner<CpuTopoNode *>(nullptr);
        }
        CpuTopology &operator=(CpuTopology &&other) noexcept {
            if (this != &other) {
                cleanup();
                _root       = util::owner<CpuTopoNode *>(other._root.get());
                other._root = util::owner<CpuTopoNode *>(nullptr);
            }
            return *this;
        }

        ~CpuTopology() {
            cleanup();
        }

        /**
         * @brief 获取全部逻辑 CPU 列表.
         */
        [[nodiscard]]
        const std::vector<cpuid_t> &logical_cpus() const {
            assert(_root != nullptr);
            return _root->cpu_ids;
        }

        /**
         * @brief 打印 CPU 拓扑树.
         */
        void print() const noexcept;

        /**
         * @brief 获取指定 CPU 在某一层级的祖先节点.
         */
        [[nodiscard]]
        Result<const CpuTopoNode *> ancestor(CpuTopoLevel level,
                                             cpuid_t cpu_id) const;

        /**
         * @brief 获取两个 CPU 共享的最高层级.
         */
        [[nodiscard]]
        CpuTopoLevel shared_level(cpuid_t cpu_id1, cpuid_t cpu_id2) const;

        /**
         * @brief 获取与指定 CPU 在某层级共享节点的其他 CPU.
         */
        [[nodiscard]]
        std::vector<cpuid_t> siblings(cpuid_t cpu_id, CpuTopoLevel level) const;
    };

    /**
     * @brief CPU 拓扑树构造器.
     */
    class CpuTopologyBuilder {
    private:
        util::owner<CpuTopoNode *> _root;

    public:
        CpuTopologyBuilder() noexcept : _root(nullptr) {}
        CpuTopologyBuilder(const CpuTopologyBuilder &)            = delete;
        CpuTopologyBuilder &operator=(const CpuTopologyBuilder &) = delete;
        CpuTopologyBuilder(CpuTopologyBuilder &&other) noexcept
            : _root(other._root.get()) {
            other._root = util::owner<CpuTopoNode *>(nullptr);
        }
        CpuTopologyBuilder &operator=(CpuTopologyBuilder &&other) noexcept {
            if (this != &other) {
                cleanup();
                _root       = util::owner<CpuTopoNode *>(other._root.get());
                other._root = util::owner<CpuTopoNode *>(nullptr);
            }
            return *this;
        }
        ~CpuTopologyBuilder() {
            cleanup();
        }

        /**
         * @brief 释放当前构建中的拓扑树.
         */
        void cleanup() noexcept {
            if (_root) {
                delete _root.get();
                _root = util::owner<CpuTopoNode *>(nullptr);
            }
        }

        /**
         * @brief 创建根节点.
         */
        CpuTopologyBuilder &root(CpuTopoLevel level, topo_t id) noexcept;
        /**
         * @brief 向指定父节点添加子节点.
         */
        [[nodiscard]]
        Result<CpuTopologyBuilder &> add_child(topo_t parent_id,
                                               CpuTopoLevel level,
                                               topo_t id) noexcept;
        /**
         * @brief 设置节点覆盖的 CPU 列表.
         */
        [[nodiscard]]
        Result<CpuTopologyBuilder &> cpus(
            topo_t node_id, std::vector<cpuid_t> cpu_ids) noexcept;
        /**
         * @brief 完成拓扑树构建.
         */
        [[nodiscard]]
        Result<CpuTopology> build() noexcept;
    };

    /**
     * @brief CPU 组信息.
     */
    struct CpuGroupInfo {
        std::vector<util::owner<Cpu *>> cpus;
        CpuTopology topology{};

        CpuGroupInfo()                                = default;
        CpuGroupInfo(const CpuGroupInfo &)            = delete;
        CpuGroupInfo &operator=(const CpuGroupInfo &) = delete;

        /**
         * @brief 移动构造 CPU 组信息.
         *
         * @param other 被移动的源对象
         */
        CpuGroupInfo(CpuGroupInfo &&other) noexcept
            : cpus(std::move(other.cpus)),
              topology(std::move(other.topology)) {}

        /**
         * @brief 移动赋值 CPU 组信息.
         *
         * @param other 被移动的源对象
         * @return CpuGroupInfo& 当前对象引用
         */
        CpuGroupInfo &operator=(CpuGroupInfo &&other) noexcept {
            if (this != &other) {
                cleanup();
                cpus     = std::move(other.cpus);
                topology = std::move(other.topology);
            }
            return *this;
        }

        /**
         * @brief 析构 CPU 组信息.
         */
        ~CpuGroupInfo() noexcept {
            cleanup();
        }

        /**
         * @brief 释放 CPU 对象.
         */
        void cleanup() noexcept {
            for (auto &cpu : cpus) {
                delete cpu.get();
            }
            cpus.clear();
        }
    };
}  // namespace device
