/**
 * @file local.cpp
 * @brief BSP CpuLocal 的常量初始化和抢占深度操作。
 */

#include <cpu/local.h>

namespace cpu {
    namespace {
        constinit tay::static_array<CpuSlot, MAX_CPUS> storage_{};
    }

    CpuSlot &bsp_storage() noexcept {
        return storage_[0];
    }

    CpuSlot *try_slot(CpuId id) noexcept {
        return id.valid() ? &storage_[id.value] : nullptr;
    }

    CpuLocal &local() noexcept {
        CpuLocal *pointer = nullptr;
#if defined(__ARCH_RISCV64__)
        asm volatile("mv %0, tp" : "=r"(pointer));
#elif defined(__ARCH_LOONGARCH64__)
        asm volatile("move %0, $r2" : "=r"(pointer));
#else
        pointer = &storage_[0].local;
#endif
        if (pointer != nullptr) {
            for (size_t index = 0; index < MAX_CPUS; ++index)
                if (pointer == &storage_[index].local)
                    return *pointer;
        }
        __builtin_trap();
    }

    CpuId current_id() noexcept {
        return local().id;
    }

    void bind_storage(CpuSlot &storage, CpuId id, CpuHwId hw_id, addr_t kernel_stack_top) noexcept {
        auto &state           = storage.local;
        state.id              = id;
        state.hw_id           = hw_id;
        state.kstack_top      = kernel_stack_top;
        state.preempt_depth   = 0;
        state.irq_depth       = 0;
        state.resched_pending = false;
        state.exec_owner      = 0;
        state.current_thread  = nullptr;
        state.bound           = true;
#if defined(__ARCH_RISCV64__)
        asm volatile("mv tp, %0" : : "r"(&state) : "memory");
#elif defined(__ARCH_LOONGARCH64__)
        asm volatile("move $r2, %0" : : "r"(&state) : "memory");
#endif
    }

    void initialize_bsp(CpuHwId hw_id, addr_t kernel_stack_top) noexcept {
        bind_storage(storage_[0], CpuId{0}, hw_id, kernel_stack_top);
        storage_[0].lifecycle.store(encode_lifecycle(CpuState::OFFLINE, 0),
                                    std::memory_order_relaxed);
    }

    void preempt_enter() noexcept {
        auto &state = local();
        if (!state.bound)
            __builtin_trap();
        auto &depth = state.preempt_depth;
        if (depth == static_cast<u32_t>(-1))
            __builtin_trap();
        ++depth;
    }

    bool preempt_leave() noexcept {
        auto &state = local();
        if (state.preempt_depth == 0)
            return false;
        --state.preempt_depth;
        return true;
    }

    bool preempt_disabled() noexcept {
        return local().preempt_depth != 0;
    }

    void defer_resched() noexcept {
        auto &state           = local();
        state.resched_pending = true;
    }

    bool take_resched() noexcept {
        auto &state           = local();
        const bool deferred   = state.resched_pending;
        state.resched_pending = false;
        return deferred;
    }

    void irq_enter() noexcept {
        auto &state = local();
        if (state.irq_depth == static_cast<u32_t>(-1))
            __builtin_trap();
        ++state.irq_depth;
    }

    bool irq_leave() noexcept {
        auto &state = local();
        if (state.irq_depth == 0)
            return false;
        --state.irq_depth;
        return true;
    }

    bool in_irq() noexcept {
        return local().irq_depth != 0;
    }

    void set_exec_owner(const void *owner) noexcept {
        local().exec_owner     = reinterpret_cast<addr_t>(owner);
        local().current_thread = static_cast<task::Thread *>(const_cast<void *>(owner));
    }

    task::Thread *current_thread() noexcept {
        return local().current_thread;
    }

    addr_t execution_token() noexcept {
        const auto owner = local().exec_owner;
        if (owner != 0)
            return owner;
        // CpuLocal 的 64-byte 对齐保证该 boot token 不会与 Thread 指针对撞。
        return reinterpret_cast<addr_t>(&local()) | addr_t{1};
    }
}  // namespace cpu
