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
#include <obj/kernel_object.h>
#include <obj/objfwd.h>
#include <scheduler/entity.h>
#include <task/state.h>
#include <task/thread_error.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/list.h>
#include <tay/spinlock.h>
#include <tay/units.h>
#include <timer/timer_node.h>

#include <cstddef>

namespace scheduler {
    class SchedulerCore;
    struct ThreadSchedAdapter;
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

        [[nodiscard]] addr_t top() const noexcept;

    private:
        explicit constexpr KernelStack(std::byte *base, size_t bytes) noexcept
            : base_(base), bytes_(bytes) {}
        void reset() noexcept;

        std::byte *base_ = nullptr;
        size_t bytes_    = 0;
    };

    class Thread final : public cap::TypedKernelObject<Thread, cap::ObjectType::THREAD> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::THREAD;

        [[nodiscard]] static Thread adopt_current(Process &process) noexcept;
        [[nodiscard]] static tay::expected<cap::ObjectRef<Thread>, ThreadError> create_kernel(
            Process &process, ThreadEntry entry, void *argument = nullptr) noexcept;
        [[nodiscard]] static tay::expected<cap::ObjectRef<Thread>, ThreadError> create_user(
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
        [[nodiscard]] bool wake_timed_wait(u64_t generation) noexcept;
        [[nodiscard]] bool cancel_timed_wait(u64_t generation) noexcept;
        [[nodiscard]] u64_t timed_wait_generation() noexcept;
        [[nodiscard]] bool timed_wait_idle() noexcept;

        [[nodiscard]] ThreadState state() const noexcept {
            return state_;
        }
        [[nodiscard]] bool exited() const noexcept {
            return state_ == ThreadState::EXITED;
        }
        [[nodiscard]] bool scheduler_attached() const noexcept {
            return scheduler_attached_;
        }
        [[nodiscard]] Process &process() const noexcept {
            return *process_;
        }
        [[nodiscard]] ThreadMode mode() const noexcept {
            return mode_;
        }
        [[nodiscard]] scheduler::SchedulerStorage &scheduler_storage() noexcept {
            return scheduler_storage_;
        }
        [[nodiscard]] const scheduler::SchedulerStorage &scheduler_storage() const noexcept {
            return scheduler_storage_;
        }

    private:
        friend class Process;
        friend class scheduler::SchedulerCore;
        friend struct scheduler::ThreadSchedAdapter;
        class TimedWaitWorklet final : public kernel::async::Worklet {
        public:
            constexpr TimedWaitWorklet() noexcept = default;

            /** @brief 在 timer reservation 的 release 发布前配置本次 typed completion。 */
            void setup(Thread &thread, u64_t generation) noexcept;

        private:
            void run() noexcept override;

            Thread *thread_   = nullptr;
            u64_t generation_ = 0;
        };

        struct adopt_current_tag final {};
        Thread(Process &process, adopt_current_tag) noexcept;
        Thread(Process &process, KernelStack &&stack, ThreadMode mode, ThreadEntry entry,
               void *argument) noexcept;
        [[nodiscard]] bool finish_timed_wait(u64_t generation, TimedWaitResult result) noexcept;

        cap::ObjectRef<Process> process_{};
        using process_hook = tay::intrusive_list_hook<Thread *, Thread *>;
        process_hook process_hook_{};
        cap::ObjectRef<Thread> scheduler_ref_{};
        KernelStack stack_{};
        hal::Context context_{};
        hal::TrapFrame user_frame_{};
        ThreadEntry entry_       = nullptr;
        void *argument_          = nullptr;
        ThreadState state_       = ThreadState::CREATED;
        ThreadMode mode_         = ThreadMode::KERNEL;
        bool configured_         = false;
        bool scheduler_attached_ = false;
        bool wake_pending_       = false;
        bool block_token_active_ = false;
        u64_t block_sequence_    = 0;
        tay::spinlock timed_wait_lock_{};
        kernel::timer::PrecisionTimerNode timed_wait_timer_{};
        TimedWaitWorklet timed_wait_worklet_{};
        u64_t timed_wait_generation_       = 0;
        u64_t timed_wait_pin_generation_   = 0;
        TimedWaitState timed_wait_state_   = TimedWaitState::IDLE;
        TimedWaitResult timed_wait_result_ = TimedWaitResult::NONE;
        bool timed_wait_pin_active_        = false;
        scheduler::SchedulerStorage scheduler_storage_{};
    };
}  // namespace task
