/**
 * @file scheduler.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Scheduler park/block 与 RR 抢占 selftest。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <test/cases.h>

#include <atomic>
#include <utility>

namespace kernel::test::cases {
    namespace {
        struct ParkState final {
            task::Thread *target = nullptr;
            bool notifier_ran    = false;
        };

        struct RrPreemptionState final {
            std::atomic<bool> release{false};
            std::atomic<bool> cpu_bound_ran{false};
            std::atomic<bool> observer_ran{false};
        };

        static_assert(std::atomic<bool>::is_always_lock_free);

        [[nodiscard]] task::Thread &current_thread(Context &context) noexcept {
            kernel::test::require(context.current_thread != nullptr,
                                  "Scheduler selftest 缺少当前 Thread");
            return *context.current_thread;
        }

        void park_notifier_entry(void *opaque) noexcept {
            auto *state = static_cast<ParkState *>(opaque);
            if (state == nullptr || state->target == nullptr ||
                state->target->state() != task::ThreadState::BLOCKED)
                kernel::test::fail("scheduler park notifier 状态无效");
            scheduler::instance().notify_runnable_work(*state->target);
            scheduler::instance().notify_runnable_work(*state->target);
            state->notifier_ran = true;
        }

        void rr_cpu_bound_entry(void *opaque) noexcept {
            auto *state = static_cast<RrPreemptionState *>(opaque);
            if (state == nullptr || !hal::interrupts_enabled())
                kernel::test::fail("RR CPU-bound Thread 未在可中断环境中运行");
            state->cpu_bound_ran.store(true, std::memory_order_release);
            while (!state->release.load(std::memory_order_acquire)) {
            }
        }

        void rr_preemption_observer(void *opaque) noexcept {
            auto *state = static_cast<RrPreemptionState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("RR preemption observer 状态无效");
            // 首次 dispatch 后可能在 entry 发布状态前立即到期；等待其真正进入忙循环后，
            // observer 的再次运行仍只能由 RR one-shot 抢占触发。
            while (!state->cpu_bound_ran.load(std::memory_order_acquire)) scheduler::yield();
            state->observer_ran.store(true, std::memory_order_release);
            state->release.store(true, std::memory_order_release);
        }
    }  // namespace

    void run_scheduler_park(Context &context) noexcept {
        auto &current = current_thread(context);
        auto &core    = scheduler::instance();
        core.notify_runnable_work(current);

        {
            hal::interrupt_guard interrupt_guard;
            auto token = core.prepare_block_current();
            if (!token)
                kernel::test::fail("scheduler BLOCKING prepare 失败");
            core.notify_runnable_work(current);
            if (auto committed = core.commit_block_current(std::move(*token)); !committed)
                kernel::test::fail("scheduler BLOCKING commit 失败");
        }
        kernel::test::require(
            current.state() == task::ThreadState::RUNNING && core.current() == &current,
            "BLOCKING 通知未取消 park");

        {
            hal::interrupt_guard interrupt_guard;
            auto token = core.prepare_block_current();
            if (!token)
                kernel::test::fail("scheduler cancel prepare 失败");
            core.cancel_block_current(std::move(*token));
        }
        kernel::test::require(
            current.state() == task::ThreadState::RUNNING && core.current() == &current,
            "BlockToken cancel 未恢复 current");

        ParkState state{.target = &current};
        auto notifier =
            task::Thread::create_kernel(task::kernel_process(), park_notifier_entry, &state);
        if (!notifier)
            kernel::test::fail("无法创建 scheduler park notifier");
        if (auto attached = core.attach(**notifier); !attached)
            kernel::test::fail("无法发布 scheduler park notifier");
        kernel::test::require(core.preemption_deadline().armed,
                              "scheduler 未为竞争者发布 RR deadline");
        if (auto blocked = core.block_current(); !blocked)
            kernel::test::fail("BLOCKED 测试未恢复 current");
        kernel::test::require(state.notifier_ran && current.state() == task::ThreadState::RUNNING &&
                                  core.current() == &current && (*notifier)->exited(),
                              "BLOCKED notification 结果不一致");
        notifier->reset();
        kernel::test::require(!core.preemption_deadline().armed,
                              "scheduler 在无竞争者时保留 RR deadline");
    }

    void run_rr_preemption(Context &) noexcept {
        RrPreemptionState state{};
        auto cpu_bound =
            task::Thread::create_kernel(task::kernel_process(), rr_cpu_bound_entry, &state);
        auto observer =
            task::Thread::create_kernel(task::kernel_process(), rr_preemption_observer, &state);
        if (!cpu_bound || !observer || !scheduler::instance().attach(**cpu_bound) ||
            !scheduler::instance().attach(**observer))
            kernel::test::fail("RR preemption 用例初始化失败");

        scheduler::yield();
        while (!(*cpu_bound)->exited() || !(*observer)->exited()) scheduler::yield();
        kernel::test::require(state.cpu_bound_ran.load(std::memory_order_acquire) &&
                                  state.observer_ran.load(std::memory_order_acquire),
                              "RR deadline 未抢占 CPU-bound kernel Thread");
        cpu_bound->reset();
        observer->reset();
    }
}  // namespace kernel::test::cases
