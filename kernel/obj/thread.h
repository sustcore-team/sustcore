/**
 * @file thread.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability Thread 调度实体、配置状态与栈所有权。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/context.h>
#include <arch/frame.h>
#include <obj/kernel_object.h>
#include <obj/objfwd.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/list.h>

#include <cstddef>

namespace scheduler {
    class RunQueue;
    class Scheduler;
    struct RQHookLocator;
}  // namespace scheduler

namespace task {
    struct ProcessThreadHookLocator;

    inline constexpr size_t KERNEL_STACK_SIZE = 64 * 1024;

    enum class ThreadState : u8_t {
        CREATED,
        SUSPENDED,
        READY,
        RUNNING,
        EXITED,
    };

    enum class ThreadMode : u8_t {
        KERNEL,
        USER,
    };

    using ThreadEntry = void (*)(void *) noexcept;

    class KernelStack final {
    public:
        [[nodiscard]] static tay::expected<KernelStack, tay::error_code> create(
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
        [[nodiscard]] static tay::expected<cap::ObjectRef<Thread>, tay::error_code> create_kernel(
            Process &process, ThreadEntry entry, void *argument = nullptr) noexcept;
        [[nodiscard]] static tay::expected<cap::ObjectRef<Thread>, tay::error_code> create_user(
            Process &process) noexcept;

        Thread(const Thread &)            = delete;
        Thread &operator=(const Thread &) = delete;
        Thread(Thread &&)                 = delete;
        Thread &operator=(Thread &&)      = delete;
        ~Thread() noexcept;

        [[nodiscard]] tay::expected<void, tay::error_code> configure_user(
            addr_t entry, addr_t stack_pointer, addr_t argument = 0) noexcept;

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

    private:
        friend class scheduler::RunQueue;
        friend class scheduler::Scheduler;
        friend struct scheduler::RQHookLocator;
        friend struct ProcessThreadHookLocator;

        struct adopt_current_tag final {};
        Thread(Process &process, adopt_current_tag) noexcept;
        Thread(Process &process, KernelStack &&stack, ThreadMode mode, ThreadEntry entry,
               void *argument) noexcept;

        using rq_hook      = tay::intrusive_list_hook<Thread *, Thread *>;
        using process_hook = tay::intrusive_list_hook<Thread *, Thread *>;

        cap::ObjectRef<Process> process_{};
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
        rq_hook rq_hook_{};
        process_hook process_hook_{};
    };
}  // namespace task
