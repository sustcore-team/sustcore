/**
 * @file thread.h
 * @brief 单核协作式调度器的内核 Thread 与栈所有权模型
 */

#pragma once

#include <arch/context.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/list.h>
#include <tay/unique_ptr.h>

#include <cstddef>

namespace scheduler {
    class RunQueue;
    class Scheduler;
    struct RQHookLocator;
}  // namespace scheduler

namespace task {
    inline constexpr size_t KERNEL_STACK_SIZE = 64 * 1024;

    enum class ThreadState : u8_t {
        SUSPENDED,
        READY,
        RUNNING,
        EXITED,
    };

    using ThreadEntry = void (*)(void *) noexcept;

    /** @brief 页对齐内核栈的独占 RAII 所有者。 */
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
        [[nodiscard]] bool owns_storage() const noexcept {
            return base_ != nullptr;
        }

    private:
        explicit constexpr KernelStack(std::byte *base, size_t bytes) noexcept
            : base_(base), bytes_(bytes) {}

        void reset() noexcept;

        std::byte *base_ = nullptr;
        size_t bytes_    = 0;
    };

    /** @brief 当前阶段唯一的可调度实体；只承载内核执行流。 */
    class Thread final {
    public:
        [[nodiscard]] static Thread adopt_current() noexcept;
        [[nodiscard]] static tay::expected<tay::unique_ptr<Thread>, tay::error_code> create_kernel(
            ThreadEntry entry, void *argument = nullptr) noexcept;

        Thread(const Thread &)            = delete;
        Thread &operator=(const Thread &) = delete;
        Thread(Thread &&)                 = delete;
        Thread &operator=(Thread &&)      = delete;
        ~Thread() noexcept;

        [[nodiscard]] ThreadState state() const noexcept {
            return state_;
        }
        [[nodiscard]] bool exited() const noexcept {
            return state_ == ThreadState::EXITED;
        }

    private:
        friend class scheduler::RunQueue;
        friend class scheduler::Scheduler;
        friend struct scheduler::RQHookLocator;

        struct adopt_current_tag final {};

        explicit constexpr Thread(adopt_current_tag) noexcept : state_(ThreadState::RUNNING) {}
        Thread(KernelStack &&stack, ThreadEntry entry, void *argument) noexcept;

        using rq_hook = tay::intrusive_list_hook<Thread *, Thread *>;

        KernelStack stack_{};
        hal::Context context_{};
        ThreadEntry entry_ = nullptr;
        void *argument_    = nullptr;
        ThreadState state_ = ThreadState::SUSPENDED;
        rq_hook rq_hook_{};
    };
}  // namespace task
