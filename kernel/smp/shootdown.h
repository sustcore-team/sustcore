/**
 * @file shootdown.h
 * @brief 全在线 CPU TLB shootdown 的串行化协调器。
 */

#pragma once

#include <cpu/topology.h>
#include <memory/virtual/pt_root.h>

#include <cstddef>

namespace smp {
    enum class TlbInvalidationKind : u8_t {
        FULL,
        RANGE,
    };

    /** @brief 已发布 shootdown 请求的诊断快照。 */
    struct TlbShootdownSnapshot final {
        u64_t generation = 0;
        cpu::CpuSet targets{};
        TlbInvalidationKind kind = TlbInvalidationKind::FULL;
        memory::RootBinding binding{};
        addr_t address = 0;
        size_t pages   = 0;
    };

    /**
     * @brief 安装 IPI 路径使用的无锁本地确认 handler。
     * @pre BSP 已发布 CPU topology 并启用本地 runtime IPI。
     */
    void init_shootdown() noexcept;

    /**
     * @brief 在已发布 PTE 修改后使所有 online CPU 的 TLB 失效。
     * @note 调用方持有 PageTable 所属锁；返回前所有目标 CPU 已确认 generation，随后才可
     *       归还脱链的页表页或依赖旧映射不可见的资源。早期 topology 尚未发布时只执行本地
     *       flush，因为此时不可能有远端 CPU 使用该映射。
     */
    void shootdown(memory::RootBinding binding, addr_t address, size_t pages,
                   TlbInvalidationKind kind = TlbInvalidationKind::FULL) noexcept;

    /** @brief runtime IPI handler 在当前 CPU 执行 full flush 并发布 acknowledgement。 */
    void handle_shootdown() noexcept;

    /** @brief 返回最后发布的请求；仅供 selftest 和故障诊断读取。 */
    [[nodiscard]] TlbShootdownSnapshot shootdown_snapshot() noexcept;
    /** @brief 读取指定 CPU 已确认的最大 generation。 */
    [[nodiscard]] u64_t acked_gen(cpu::CpuId id) noexcept;
}  // namespace smp
