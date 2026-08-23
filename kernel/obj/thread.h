/**
 * @file thread.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability Thread 调度实体、内核栈、配置状态与 timed wait 所有权。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/context.h>
#include <arch/frame.h>
#include <async/worklet.h>
#include <obj/kobject.h>
#include <obj/objfwd.h>
#include <scheduler/entity.h>
#include <synchronized.h>
#include <task/state.h>
#include <task/thread_error.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/list.h>
#include <tay/units.h>
#include <timer/node.h>

#include <atomic>
#include <cstddef>

namespace scheduler {
    class SchedCore;
    struct ThreadSchedOps;
}  // namespace scheduler

namespace task {
    struct ProcessThreadHookLocator;

    inline constexpr size_t KERNEL_STACK_SIZE = 64 * 1024;

    using ThreadEntry = void (*)(void *) noexcept;

    class KernelStack final {
    public:
        [[nodiscard]] static tay::expected<KernelStack, ThreadError> create(
            size_t bytes = KERNEL_STACK_SIZE) noexcept;

        constexpr KernelStack() noexcept            = default;
        KernelStack(const KernelStack &)            = delete;
        KernelStack &operator=(const KernelStack &) = delete;
        KernelStack(KernelStack &&other) noexcept;
        KernelStack &operator=(KernelStack &&other) noexcept;
        ~KernelStack() noexcept;

        [[nodiscard]] bool valid() const noexcept {
            return base_ != nullptr;
        }

        [[nodiscard]] addr_t top() const noexcept;

    private:
        explicit constexpr KernelStack(std::byte *base, size_t bytes) noexcept
            : base_(base), bytes_(bytes) {}
        void reset() noexcept;

        std::byte *base_ = nullptr;
        size_t bytes_    = 0;
    };

    class Thread final : public cap::TypedKObject<Thread, cap::ObjectType::THREAD> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::THREAD;

        [[nodiscard]] static Thread adopt_current(Process &process) noexcept;
        [[nodiscard]] static tay::expected<cap::KObjectRef<Thread>, ThreadError> create_kernel(
            Process &process, ThreadEntry entry, void *argument = nullptr) noexcept;
        /**
         * @brief 为已提交的用户 Process 创建受内核控制的 worker Thread。
         * @note 仅供内核 selftest/受信任 subsystem 使用；普通用户线程仍必须走 create_user。
         */
        [[nodiscard]] static tay::expected<cap::KObjectRef<Thread>, ThreadError> create_kernel_for(
            Process &process, ThreadEntry entry, void *argument = nullptr) noexcept;
        [[nodiscard]] static tay::expected<cap::KObjectRef<Thread>, ThreadError> create_user(
            Process &process) noexcept;

        Thread(const Thread &)            = delete;
        Thread &operator=(const Thread &) = delete;
        Thread(Thread &&)                 = delete;
        Thread &operator=(Thread &&)      = delete;
        ~Thread() noexcept;

        [[nodiscard]] tay::expected<void, ThreadError> configure_user(addr_t entry,
                                                                      addr_t stack_pointer,
                                                                      addr_t argument = 0) noexcept;

        /** @brief 让 current Thread 等待到绝对 deadline；第一版每个 Thread 只允许一个。 */
        [[nodiscard]] tay::expected<TimedWaitResult, ThreadError> wait_until(
            units::time deadline) noexcept;
        /** @brief 由显式事件竞争当前 generation 的完成权；winner 唤醒 waiter。 */
        [[nodiscard]] bool wake_wait(u64_t generation) noexcept;
        [[nodiscard]] bool cancel_wait(u64_t generation) noexcept;
        [[nodiscard]] u64_t wait_gen() noexcept;
        [[nodiscard]] bool wait_idle() noexcept;

        /**
         * @brief 复制 scheduler 在目标 run queue lock 内发布的生命周期快照。
         * @note 快照只用于跨 CPU 观察，不能据此修改 Thread 或规避目标队列的所有权协议。
         */
        [[nodiscard]] ThreadState state() const noexcept {
            return sched_snapshot_.load(std::memory_order_acquire);
        }
        [[nodiscard]] bool exited() const noexcept {
            return state() == ThreadState::EXITED;
        }
        [[nodiscard]] bool scheduler_attached() const noexcept {
            return sched_attached_;
        }
        [[nodiscard]] u32_t sched_cpu() const noexcept {
            return sched_cpu_.load(std::memory_order_acquire);
        }
        [[nodiscard]] Process &process() const noexcept {
            return *process_;
        }
        [[nodiscard]] ThreadMode mode() const noexcept {
            return mode_;
        }
        [[nodiscard]] scheduler::SchedStorage &scheduler_storage() noexcept {
            return sched_;
        }
        [[nodiscard]] const scheduler::SchedStorage &scheduler_storage() const noexcept {
            return sched_;
        }

    private:
        friend class Process;
        friend class scheduler::SchedCore;
        friend struct scheduler::ThreadSchedOps;
        class WaitWorklet final : public kernel::async::Worklet {
        public:
            constexpr WaitWorklet() noexcept = default;

            /** @brief 在 timer reservation 的 release 发布前配置本次 typed completion。 */
            void setup(Thread &thread, u64_t generation) noexcept;
            void clear_owner() noexcept {
                owner_ref_.reset();
            }

        private:
            void run() noexcept override;

            Thread *thread_   = nullptr;
            u64_t generation_ = 0;
            // 在 timer node 从 IDLE 到 completion dispatch 的整个期间保持 Thread 强引用。
            cap::KObjectRef<Thread> owner_ref_{};
        };

        struct adopt_current_tag final {};
        Thread(Process &process, adopt_current_tag) noexcept;
        Thread(Process &process, KernelStack &&stack, ThreadMode mode, ThreadEntry entry,
               void *argument) noexcept;
        [[nodiscard]] bool finish_wait(u64_t generation, TimedWaitResult result) noexcept;

        cap::KObjectRef<Process> process_{};
        using process_hook = tay::intrusive_list_hook<Thread *, Thread *>;
        process_hook process_hook_{};
        cap::KObjectRef<Thread> sched_ref_{};
        KernelStack stack_{};
        hal::Context context_{};
        hal::TrapFrame user_frame_{};
        ThreadEntry entry_ = nullptr;
        void *argument_    = nullptr;
        // state_ 由固定 target run queue lock 保护；跨 CPU 读者只能使用 release/acquire 快照。
        ThreadState state_ = ThreadState::CREATED;
        std::atomic<ThreadState> sched_snapshot_{ThreadState::CREATED};
        ThreadMode mode_ = ThreadMode::KERNEL;
        bool configured_ = false;
        // scheduler owner 在固定 run queue lock 内修改；公开查询只需观察发布快照。
        std::atomic<bool> sched_attached_{false};
        std::atomic<u32_t> sched_cpu_{cpu::INVALID_CPU};
        bool wake_pending_       = false;
        bool block_token_active_ = false;
        u64_t block_sequence_    = 0;

        struct WaitState final {
            u64_t generation       = 0;
            u64_t pin_generation   = 0;
            TimedWaitState state   = TimedWaitState::IDLE;
            TimedWaitResult result = TimedWaitResult::NONE;
            bool pin_active        = false;
            bool parked            = false;
        };

        kernel::irq_simple_synchronized<WaitState> wait_{};
        kernel::timer::HrTimer wait_timer_{};
        WaitWorklet wait_work_{};
        scheduler::SchedStorage sched_{};
    };
}  // namespace task
