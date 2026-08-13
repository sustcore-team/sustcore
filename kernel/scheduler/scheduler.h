/**
 * @file scheduler.h
 * @brief BSP 单核协作式 FIFO 调度器
 */

#pragma once

#include <obj/thread.h>
#include <scheduler/rq.h>
#include <tay/err.h>
#include <tay/expected.h>

namespace scheduler {
    class Scheduler final {
    public:
        constexpr Scheduler() noexcept = default;

        Scheduler(const Scheduler &)            = delete;
        Scheduler &operator=(const Scheduler &) = delete;
        Scheduler(Scheduler &&)                 = delete;
        Scheduler &operator=(Scheduler &&)      = delete;

        [[nodiscard]] tay::expected<void, tay::error_code> initialize(
            task::Thread &bootstrap) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> resume(task::Thread &thread) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> suspend(task::Thread &thread) noexcept;

        void yield() noexcept;
        [[noreturn]] void exit_current() noexcept;
        [[noreturn]] void bootstrap_current() noexcept;

        [[nodiscard]] bool ready() const noexcept {
            return ready_;
        }
        [[nodiscard]] task::Thread *current() const noexcept {
            return current_;
        }

    private:
        [[nodiscard]] task::Thread *pick_next() noexcept;
        void switch_to(task::Thread &previous, task::Thread &next) noexcept;

        RunQueue rq_{};
        cap::ObjectRef<task::Thread> deferred_exit_{};
        task::Thread *current_ = nullptr;
        bool ready_            = false;
    };

    Scheduler &instance() noexcept;
    void yield() noexcept;
    [[noreturn]] void exit_current() noexcept;

    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept;
}  // namespace scheduler
