/**
 * @file ap.h
 * @brief BSP/AP 启动状态机和 release gate。
 */

#pragma once

#include <boot/smp.h>
#include <cpu/topology.h>
#include <tay/expected.h>

namespace smp {
    class ApManager final {
    public:
        [[nodiscard]] tay::expected<void, tay::error_code> prepare(
            cpu::CpuId id, cpu::CpuHwId hw_id, PhyAddr root_pt, addr_t stack_top, addr_t cpu_local,
            addr_t entry) noexcept;

        [[nodiscard]] tay::expected<void, tay::error_code> start(
            cpu::CpuId id, boot::smp::StartAp starter) noexcept;

        [[nodiscard]] bool publish_started(const boot::smp::ApBootArgs &arguments) noexcept;
        [[nodiscard]] bool publish_ready(cpu::CpuId id, u32_t gen) noexcept;
        [[nodiscard]] bool fail(cpu::CpuId id, u32_t gen) noexcept;
        [[nodiscard]] bool abandon(cpu::CpuId id, u32_t gen) noexcept;

        /** @brief 收集 READY 集合并打开 release gate；只能成功提交一次。 */
        [[nodiscard]] bool commit_ready_set() noexcept;
        [[nodiscard]] bool committed() const noexcept;
        [[nodiscard]] bool released(cpu::CpuId id, u32_t gen) const noexcept;
        [[nodiscard]] bool publish_online(cpu::CpuId id, u32_t gen) noexcept;

        [[nodiscard]] boot::smp::StartState state(cpu::CpuId id) const noexcept;
        [[nodiscard]] u32_t generation(cpu::CpuId id) const noexcept;
        [[nodiscard]] const boot::smp::ApBootRes *boot_res(cpu::CpuId id) const noexcept;
        [[nodiscard]] boot::smp::ApBootRes *boot_res(cpu::CpuId id) noexcept;

        [[nodiscard]] cpu::CpuSet ready_set() const noexcept;
        [[nodiscard]] cpu::CpuSet committed_set() const noexcept;
    };

    [[nodiscard]] ApManager &ap_manager() noexcept;

    /** @brief AP 统一 C++ 入口，架构 trampoline 只调用此函数。 */
    extern "C" [[noreturn]] void ap_main(const boot::smp::ApBootArgs *arguments) noexcept;
}  // namespace smp
