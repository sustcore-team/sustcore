/**
 * @file smp.h
 * @brief 次级 CPU 启动协议与架构后端的窄接口。
 */

#pragma once

#include <cpu/local.h>
#include <sustcore/addr.h>
#include <sustcore/addrspace.h>
#include <tay/expected.h>

#include <atomic>
#include <cstddef>

namespace boot::smp {
    inline constexpr u64_t AP_BOOT_MAGIC       = 0x53555354434f5245ULL;
    inline constexpr u32_t AP_BOOT_ABI_VERSION = 1;
    inline constexpr size_t AP_ARCH_DATA_SIZE  = 256;

    /** @brief trampoline 进入统一 C++ 入口所需的、固定布局参数。 */
    struct alignas(16) ApBootArgs final {
        u64_t magic       = AP_BOOT_MAGIC;
        u32_t abi_version = AP_BOOT_ABI_VERSION;
        cpu::CpuId cpu_id{};
        cpu::CpuHwId hw_id{};
        PhyAddr root_pt{};
        addr_t stack_top = 0;
        addr_t cpu_local = 0;
        addr_t entry_pc  = 0;
        u32_t gen        = 0;
    };

    static_assert(offsetof(ApBootArgs, magic) == 0);
    static_assert(offsetof(ApBootArgs, abi_version) == 8);
    static_assert(offsetof(ApBootArgs, cpu_id) == 12);
    static_assert(offsetof(ApBootArgs, hw_id) == 16);
    static_assert(offsetof(ApBootArgs, root_pt) == 24);
    static_assert(offsetof(ApBootArgs, stack_top) == 32);
    static_assert(offsetof(ApBootArgs, cpu_local) == 40);
    static_assert(offsetof(ApBootArgs, entry_pc) == 48);
    static_assert(offsetof(ApBootArgs, gen) == 56);
    static_assert(sizeof(ApBootArgs) == 64);

    /**
     * @brief 每个逻辑 CPU 永久保留的启动资源。
     *
     * 参数在启动请求前由 BSP 写入，随后以 state 的 release 发布；AP 只在 acquire
     * 观察到 STARTING 后读取普通字段。arch_data 用于 MMU 前的架构 scratch，
     * 启动结果不确定时也不得回收。
     */
    struct alignas(64) ApBootRes final {
        ApBootArgs arguments{};
        std::atomic<u32_t> state{0};
        std::atomic<u32_t> gen{0};
        std::atomic<bool> release_gate{false};
        std::atomic<bool> online_ack{false};
        alignas(16) u8_t arch_data[AP_ARCH_DATA_SIZE]{};
    };

    static_assert(offsetof(ApBootRes, arguments) == 0);

    enum class StartState : u8_t {
        OFFLINE,
        PREPARED,
        STARTING,
        EARLY_ONLINE,
        READY,
        ONLINE,
        FAILED,
        ABANDONED,
    };

    using StartAp = tay::expected<void, tay::error_code> (*)(cpu::CpuHwId hw_id,
                                                             PhyAddr arguments_physical) noexcept;

    /** @brief 探测架构是否提供可用的次级 CPU 启动机制。 */
    [[nodiscard]] bool supports_ap_start() noexcept;

    /** @brief 启动一个已准备好的次级 CPU；不负责修改通用状态机。 */
    [[nodiscard]] tay::expected<void, tay::error_code> start_ap(
        cpu::CpuHwId hw_id, PhyAddr arguments_physical) noexcept;

    /** @brief 返回常驻 trampoline 所在的物理页。 */
    [[nodiscard]] PhyAddr trampoline_page() noexcept;
}  // namespace boot::smp
