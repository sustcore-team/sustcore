/**
 * @file local.h
 * @brief 当前 CPU 的固定局部状态与抢占深度。
 */

#pragma once

#include <smp/mailbox.h>
#include <tay/array.h>
#include <tay/bits.h>

#include <atomic>
#include <cstddef>

namespace memory {
    class UserVm;
}

namespace task {
    class Thread;
}

namespace cpu {
    inline constexpr size_t MAX_CPUS   = 64;
    inline constexpr u32_t INVALID_CPU = static_cast<u32_t>(-1);

    struct CpuId final {
        u32_t value = INVALID_CPU;

        [[nodiscard]] constexpr bool valid() const noexcept {
            return value < MAX_CPUS;
        }

        [[nodiscard]] friend constexpr bool operator==(CpuId, CpuId) noexcept = default;
    };

    struct CpuHwId final {
        u64_t value = 0;

        [[nodiscard]] friend constexpr bool operator==(CpuHwId, CpuHwId) noexcept = default;
    };

    /**
     * @brief 热路径读取的当前 CPU 状态。
     *
     * 第一阶段只有 BSP 使用该对象；对象地址从启动早期开始稳定，后续阶段会把同一布局扩展为
     * 固定 CpuSlot 的前缀，避免通过动态容器移动 CPU 局部状态。
     */
    struct alignas(64) CpuLocal final {
        CpuId id{};
        CpuHwId hw_id{};
        addr_t kstack_top    = 0;
        u32_t preempt_depth  = 0;
        u32_t irq_depth      = 0;
        bool resched_pending = false;
        bool bound           = false;

        // trap entry 在切换到当前 kernel stack 前暂存被覆盖的寄存器。该区域只在本 CPU 的
        // trap 入口短暂使用，TrapFrame 建立后即失效；禁止普通 C++ 代码依赖这些字段。
        addr_t trap_saved_sp           = 0;
        addr_t trap_saved_t0           = 0;
        addr_t trap_saved_t1           = 0;
        addr_t trap_saved_tp           = 0;
        memory::UserVm *active_user_vm = nullptr;
        // 非零时指向当前 Thread；调度器就绪前 execution_token() 使用 CpuLocal 地址派生的 BSP/AP
        // boot token。该值只由本 CPU 的 IRQ-off scheduler 提交路径写入。
        addr_t exec_owner              = 0;
        task::Thread *current_thread   = nullptr;
    };

    static_assert(offsetof(CpuLocal, kstack_top) == 16);
    static_assert(offsetof(CpuLocal, trap_saved_sp) == 40);
    static_assert(offsetof(CpuLocal, trap_saved_t0) == 48);
    static_assert(offsetof(CpuLocal, trap_saved_t1) == 56);
    static_assert(offsetof(CpuLocal, trap_saved_tp) == 64);

    enum class CpuState : u8_t {
        OFFLINE,
        POSSIBLE,
        STARTED,
        READY,
        ONLINE,
        FAILED,
    };

    [[nodiscard]] constexpr u64_t encode_lifecycle(CpuState state, u32_t generation) noexcept {
        return (static_cast<u64_t>(generation) << 8) | static_cast<u8_t>(state);
    }

    [[nodiscard]] constexpr CpuState lifecycle_state(u64_t value) noexcept {
        return static_cast<CpuState>(value & 0xff);
    }

    [[nodiscard]] constexpr u32_t lifecycle_gen(u64_t value) noexcept {
        return static_cast<u32_t>(value >> 8);
    }

    struct alignas(64) CpuSlot final {
        std::atomic<u64_t> lifecycle{encode_lifecycle(CpuState::OFFLINE, 0)};
        CpuLocal local{};
        smp::IpiMailbox ipi{};
    };

    /** @brief 返回固定地址的 BSP storage。当前阶段不会移动或替换该对象。 */
    [[nodiscard]] CpuSlot &bsp_storage() noexcept;
    [[nodiscard]] CpuSlot *try_slot(CpuId id) noexcept;
    [[nodiscard]] CpuLocal &local() noexcept;
    [[nodiscard]] CpuId current_id() noexcept;

    void bind_storage(CpuSlot &storage, CpuId id, CpuHwId hw_id, addr_t kernel_stack_top) noexcept;
    void initialize_bsp(CpuHwId hw_id, addr_t kernel_stack_top) noexcept;

    void preempt_enter() noexcept;
    [[nodiscard]] bool preempt_leave() noexcept;
    [[nodiscard]] bool preempt_disabled() noexcept;
    void defer_resched() noexcept;
    [[nodiscard]] bool take_resched() noexcept;

    void irq_enter() noexcept;
    [[nodiscard]] bool irq_leave() noexcept;
    [[nodiscard]] bool in_irq() noexcept;

    /** @brief 在当前作用域记录硬中断嵌套深度，并在退出时验证配对。 */
    class irq_context_guard final {
    public:
        irq_context_guard() noexcept {
            irq_enter();
        }
        ~irq_context_guard() noexcept {
            if (!irq_leave())
                __builtin_trap();
        }

        irq_context_guard(const irq_context_guard &)            = delete;
        irq_context_guard &operator=(const irq_context_guard &) = delete;
        irq_context_guard(irq_context_guard &&)                 = delete;
        irq_context_guard &operator=(irq_context_guard &&)      = delete;
    };

    /** @brief 发布当前执行 Thread 的身份；传入 nullptr 恢复启动期 CPU token。 */
    void set_exec_owner(const void *owner) noexcept;
    [[nodiscard]] task::Thread *current_thread() noexcept;
    /** @brief 返回用于 C++ 局部 static 所有者判定的 Thread 或启动期执行 token。 */
    [[nodiscard]] addr_t execution_token() noexcept;
}  // namespace cpu
