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
#include <arch/timer.h>
#include <cpu/topology.h>
#include <log.h>
#include <memory/virtual/kernel/vm.h>
#include <obj/addr_space.h>
#include <obj/cspace.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <smp/ap.h>
#include <smp/ipi.h>
#include <smp/shootdown.h>
#include <synchronized.h>
#include <test/cases.h>
#include <timer/deadline.h>
#include <timer/hrtimer.h>

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

        struct RemoteWakeState final {
            std::atomic<bool> entered{false};
            std::atomic<bool> resumed{false};
            std::atomic<u32_t> resumed_cpu{cpu::INVALID_CPU};
        };

        struct LocalPreemptionState final {
            std::atomic<bool> cpu_bound_entered{false};
            std::atomic<bool> observer_ran{false};
            std::atomic<bool> release{false};
            std::atomic<u64_t> observed_preemptions{0};
        };

        struct RemoteTimedWaitState final {
            std::atomic<bool> waiting{false};
            std::atomic<bool> completed{false};
            std::atomic<u32_t> completed_cpu{cpu::INVALID_CPU};
            std::atomic<u32_t> result{static_cast<u32_t>(task::TimedWaitResult::NONE)};
        };

        struct AddressSpaceSwitchState final {
            std::atomic<u64_t> completed_mask{0};
            std::atomic<u64_t> active_mask{0};
        };

        struct CounterState final {
            u64_t value = 0;
            u64_t by_cpu[cpu::MAX_CPUS]{};
        };

        struct PinnedWorkState final {
            kernel::synchronized<CounterState> counter{};
            std::atomic<size_t> completed_count{0};
            std::atomic<u64_t> observed_mask{0};
        };

        struct AnyWorkState final {
            std::atomic<bool> release{false};
            std::atomic<size_t> started_count{0};
            std::atomic<u64_t> started_mask{0};
        };

        struct RemoteShootdownState final {
            std::atomic<bool> completed{false};
            std::atomic<u64_t> generation{0};
            std::atomic<u32_t> cpu{cpu::INVALID_CPU};
        };

        static_assert(std::atomic<bool>::is_always_lock_free);
        static_assert(std::atomic<u32_t>::is_always_lock_free);
        static_assert(std::atomic<u64_t>::is_always_lock_free);

        [[nodiscard]] task::Thread &current_thread(Context &context) noexcept {
            kernel::test::require(context.current_thread != nullptr,
                                  "Scheduler selftest 缺少当前 Thread");
            return *context.current_thread;
        }

        void park_notify(void *opaque) noexcept {
            auto *state = static_cast<ParkState *>(opaque);
            if (state == nullptr || state->target == nullptr ||
                state->target->state() != task::ThreadState::BLOCKED)
                kernel::test::fail("scheduler park notifier 状态无效");
            scheduler::notify_work(*state->target);
            scheduler::notify_work(*state->target);
            state->notifier_ran = true;
        }

        void rr_cpu_bound_entry(void *opaque) noexcept {
            auto *state = static_cast<RrPreemptionState *>(opaque);
            if (state == nullptr || !hal::irq_enabled())
                kernel::test::fail("RR CPU-bound Thread 未在可中断环境中运行");
            state->cpu_bound_ran.store(true, std::memory_order_release);
            while (!state->release.load(std::memory_order_acquire)) {
            }
        }

        void rr_preempt_probe(void *opaque) noexcept {
            auto *state = static_cast<RrPreemptionState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("RR preemption observer 状态无效");
            // 首次 dispatch 后可能在 entry 发布状态前立即到期；等待其真正进入忙循环后，
            // observer 的再次运行仍只能由 RR one-shot 抢占触发。
            while (!state->cpu_bound_ran.load(std::memory_order_acquire)) scheduler::yield();
            state->observer_ran.store(true, std::memory_order_release);
            state->release.store(true, std::memory_order_release);
        }

        void blocker_entry(void *opaque) noexcept {
            auto *state = static_cast<RemoteWakeState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("remote wake worker 缺少状态");
            state->entered.store(true, std::memory_order_release);
            if (auto blocked = scheduler::local().block(); !blocked)
                kernel::test::fail("remote wake worker 无法 block");
            state->resumed_cpu.store(cpu::current_id().value, std::memory_order_release);
            state->resumed.store(true, std::memory_order_release);
        }

        void local_bound_entry(void *opaque) noexcept {
            auto *state = static_cast<LocalPreemptionState *>(opaque);
            if (state == nullptr || !hal::irq_enabled())
                kernel::test::fail("AP CPU-bound worker 缺少可中断运行环境");
            state->cpu_bound_entered.store(true, std::memory_order_release);
            while (!state->release.load(std::memory_order_acquire)) {
            }
        }

        void local_preempt(void *opaque) noexcept {
            auto *state = static_cast<LocalPreemptionState *>(opaque);
            if (state == nullptr || !state->cpu_bound_entered.load(std::memory_order_acquire))
                kernel::test::fail("AP preemption observer 没有观察到 CPU-bound worker");
            const auto before = scheduler::local().debug_state().preemption_count;
            // observer 初次运行可能由 attach 的 RESCHEDULE IPI 触发；这里不主动 yield，而是
            // 保持 ready competitor 存在，等待本地 RR deadline 使 observer 再次恢复。只有
            // 目标 CPU 的 timer IRQ 能在该循环期间增加 preemption_count。
            while (scheduler::local().debug_state().preemption_count == before) {
                asm volatile("" ::: "memory");
            }
            const auto after = scheduler::local().debug_state().preemption_count;
            state->observed_preemptions.store(after - before, std::memory_order_release);
            state->release.store(true, std::memory_order_release);
            state->observer_ran.store(true, std::memory_order_release);
        }

        void remote_wait_entry(void *opaque) noexcept {
            auto *state = static_cast<RemoteTimedWaitState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("remote timed-wait worker 缺少状态");
            state->waiting.store(true, std::memory_order_release);
            auto result = scheduler::local().current_thread().wait_until(
                kernel::timer::deadline_after(hal::Clock::instance().now(), 2_ms));
            if (!result)
                kernel::test::fail("remote timed-wait worker 失败");
            state->completed_cpu.store(cpu::current_id().value, std::memory_order_release);
            state->result.store(static_cast<u32_t>(*result), std::memory_order_release);
            state->completed.store(true, std::memory_order_release);
        }

        void addr_switch_entry(void *opaque) noexcept {
            auto *state = static_cast<AddressSpaceSwitchState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("address-space worker 状态无效");
            auto *thread = scheduler::current();
            if (thread == nullptr || thread->process().kernel() ||
                thread->process().addr_space() == nullptr)
                kernel::test::fail("address-space worker 未绑定用户 Process");
            const auto id = cpu::current_id();
            if (!thread->process().addr_space()->active_local())
                kernel::test::fail("同一 Process 的地址空间未在当前 CPU 激活");
            state->active_mask.fetch_or(u64_t{1} << id.value, std::memory_order_acq_rel);
            state->completed_mask.fetch_or(u64_t{1} << id.value, std::memory_order_release);
        }

        void pinned_work_entry(void *opaque) noexcept {
            auto *state = static_cast<PinnedWorkState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("pinned work worker 状态无效");
            const auto id = cpu::current_id();
            state->observed_mask.fetch_or(u64_t{1} << id.value, std::memory_order_acq_rel);
            for (size_t index = 0; index < 256; ++index) {
                auto locked = state->counter.lock();
                if (!cpu::preempt_disabled() || !hal::irq_enabled())
                    kernel::test::fail("synchronized counter 临界区未满足 guard 契约");
                ++locked->value;
                ++locked->by_cpu[id.value];
            }
            state->completed_count.fetch_add(1, std::memory_order_release);
        }

        void any_work_entry(void *opaque) noexcept {
            auto *state = static_cast<AnyWorkState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("any placement worker 状态无效");
            state->started_mask.fetch_or(u64_t{1} << cpu::current_id().value,
                                         std::memory_order_acq_rel);
            state->started_count.fetch_add(1, std::memory_order_release);
            while (!state->release.load(std::memory_order_acquire)) asm volatile("" ::: "memory");
        }

        void shootdown_entry(void *opaque) noexcept {
            auto *state = static_cast<RemoteShootdownState *>(opaque);
            if (state == nullptr)
                kernel::test::fail("remote shootdown worker 状态无效");
            smp::shootdown(memory::kernel_vm().binding(), 0, 0);
            state->generation.store(smp::shootdown_snapshot().generation,
                                    std::memory_order_release);
            state->cpu.store(cpu::current_id().value, std::memory_order_release);
            state->completed.store(true, std::memory_order_release);
        }

        void log_sched_watchdog(const char *case_name) noexcept {
            const auto snapshot = cpu::topology().snapshot();
            kernel::log::info("scheduler watchdog: case={}, current_cpu={}", case_name,
                              cpu::current_id().value);
            snapshot.online.for_each([](cpu::CpuId id) noexcept {
                const auto stats = smp::stats(id);
                kernel::log::info("  cpu={} ipi(posted={},dispatch={},resched={},tlb={})", id.value,
                                  stats.posted, stats.dispatches, stats.reschedules,
                                  stats.tlb_shootdowns);
            });
        }

        void await_remote(const std::atomic<bool> &flag, units::time deadline,
                          const char *message) noexcept {
            while (!flag.load(std::memory_order_acquire)) {
                if (hal::Clock::instance().now() >= deadline) {
                    log_sched_watchdog(message);
                    kernel::test::fail(message);
                }
                scheduler::yield();
            }
        }
    }  // namespace

    void run_scheduler_park(Context &context) noexcept {
        auto &current = current_thread(context);
        auto &core    = scheduler::local();
        core.notify_work(current);

        {
            hal::irq_guard irq_guard;
            auto token = core.prepare_block();
            if (!token)
                kernel::test::fail("scheduler BLOCKING prepare 失败");
            core.notify_work(current);
            if (auto committed = core.commit_block(std::move(*token)); !committed)
                kernel::test::fail("scheduler BLOCKING commit 失败");
        }
        kernel::test::require(
            current.state() == task::ThreadState::RUNNING && core.current() == &current,
            "BLOCKING 通知未取消 park");

        {
            hal::irq_guard irq_guard;
            auto token = core.prepare_block();
            if (!token)
                kernel::test::fail("scheduler cancel prepare 失败");
            core.cancel_block(std::move(*token));
        }
        kernel::test::require(
            current.state() == task::ThreadState::RUNNING && core.current() == &current,
            "BlockToken cancel 未恢复 current");

        ParkState state{.target = &current};
        auto notifier =
            task::Thread::create_kernel(task::kernel_proc(), park_notify, &state);
        if (!notifier)
            kernel::test::fail("无法创建 scheduler park notifier");
        if (auto attached = core.attach(**notifier); !attached)
            kernel::test::fail("无法发布 scheduler park notifier");
        kernel::test::require(core.preempt_deadline().armed,
                              "scheduler 未为竞争者发布 RR deadline");
        if (auto blocked = core.block(); !blocked)
            kernel::test::fail("BLOCKED 测试未恢复 current");
        kernel::test::require(state.notifier_ran && current.state() == task::ThreadState::RUNNING &&
                                  core.current() == &current && (*notifier)->exited(),
                              "BLOCKED notification 结果不一致");
        notifier->reset();
        kernel::test::require(!core.preempt_deadline().armed,
                              "scheduler 在无竞争者时保留 RR deadline");
    }

    void run_rr_preemption(Context &) noexcept {
        RrPreemptionState state{};
        auto cpu_bound =
            task::Thread::create_kernel(task::kernel_proc(), rr_cpu_bound_entry, &state);
        auto observer =
            task::Thread::create_kernel(task::kernel_proc(), rr_preempt_probe, &state);
        if (!cpu_bound || !observer || !scheduler::attach(**cpu_bound) ||
            !scheduler::attach(**observer))
            kernel::test::fail("RR preemption 用例初始化失败");

        scheduler::yield();
        const auto deadline = hal::Clock::instance().now() + 2_s;
        while (!(*cpu_bound)->exited() || !(*observer)->exited()) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("rr_preemption.exit");
                kernel::test::fail("RR preemption worker 未退出");
            }
            scheduler::yield();
        }
        kernel::test::require(state.cpu_bound_ran.load(std::memory_order_acquire) &&
                                  state.observer_ran.load(std::memory_order_acquire),
                              "RR deadline 未抢占 CPU-bound kernel Thread");
        cpu_bound->reset();
        observer->reset();
    }

    void run_remote_wake(Context &context) noexcept {
        (void)current_thread(context);
        const auto online = cpu::topology().snapshot().online;
        if (online.count() < 2)
            return;
        const bool interrupts_were_enabled = hal::irq_enabled();
        if (!interrupts_were_enabled)
            hal::sti();
        const auto target = online.without(cpu::CpuId{0}).first();
        kernel::test::require(target.value != cpu::INVALID_CPU,
                              "online AP 集合缺少 remote wake 目标");

        RemoteWakeState state{};
        auto worker = task::Thread::create_kernel(task::kernel_proc(), blocker_entry, &state);
        if (!worker)
            kernel::test::fail("无法创建 remote wake worker");
        if (auto attached = scheduler::attach(**worker, scheduler::Placement::Pinned(target));
            !attached)
            kernel::test::fail("无法固定发布 remote wake worker");
        kernel::test::require((*worker)->sched_cpu() == target.value,
                              "remote wake worker 未发布固定 target owner");

        auto deadline = hal::Clock::instance().now() + 200_ms;
        await_remote(state.entered, deadline, "remote wake worker 未在目标 CPU 启动");
        while ((*worker)->state() != task::ThreadState::BLOCKED) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("remote_wake.blocked");
                kernel::test::fail("remote wake worker 未进入 BLOCKED");
            }
            scheduler::yield();
        }

        const auto before = smp::stats(target);
        scheduler::wake(**worker);
        await_remote(state.resumed, deadline, "remote wake worker 未被 RESCHEDULE IPI 恢复");
        while (!(*worker)->exited()) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("remote_wake.exit");
                kernel::test::fail("remote wake worker 未退出");
            }
            scheduler::yield();
        }
        const auto after = smp::stats(target);
        kernel::test::require(state.resumed_cpu.load(std::memory_order_acquire) == target.value,
                              "remote wake worker 未在固定 target CPU 恢复");
        kernel::test::require(
            after.posted > before.posted && after.reschedules > before.reschedules,
            "remote wake 未产生目标 RESCHEDULE IPI");
        worker->reset();

        LocalPreemptionState preemption{};
        deadline = hal::Clock::instance().now() + 200_ms;
        auto cpu_bound =
            task::Thread::create_kernel(task::kernel_proc(), local_bound_entry, &preemption);
        if (!cpu_bound || !scheduler::attach(**cpu_bound, scheduler::Placement::Pinned(target)))
            kernel::test::fail("无法创建 AP CPU-bound worker");
        await_remote(preemption.cpu_bound_entered, deadline, "AP CPU-bound worker 未启动");

        auto observer =
            task::Thread::create_kernel(task::kernel_proc(), local_preempt, &preemption);
        if (!observer || !scheduler::attach(**observer, scheduler::Placement::Pinned(target)))
            kernel::test::fail("无法创建 AP preemption observer");
        await_remote(preemption.observer_ran, deadline,
                     "AP local timer 未恢复 preemption observer");
        kernel::test::require(preemption.observed_preemptions.load(std::memory_order_acquire) != 0,
                              "AP observer 未观察到本地 timer RR 抢占");
        while (!(*cpu_bound)->exited() || !(*observer)->exited()) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("remote_preemption.exit");
                kernel::test::fail("AP local preemption worker 未退出");
            }
            scheduler::yield();
        }
        cpu_bound->reset();
        observer->reset();

        RemoteTimedWaitState timed_wait{};
        deadline                = hal::Clock::instance().now() + 200_ms;
        const auto timer_before = smp::stats(cpu::CpuId{0});
        auto timed_worker =
            task::Thread::create_kernel(task::kernel_proc(), remote_wait_entry, &timed_wait);
        if (!timed_worker ||
            !scheduler::attach(**timed_worker, scheduler::Placement::Pinned(target)))
            kernel::test::fail("无法创建 remote timed-wait worker");
        while (!timed_wait.waiting.load(std::memory_order_acquire)) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("remote_timed_wait.start");
                kernel::test::fail("remote timed-wait worker 未进入 wait_until");
            }
            scheduler::yield();
        }
        // 丢弃测试方的强引用，专门验证 BSP completion pin/ref 在远端 waiter 退出时仍覆盖
        // Thread、TimerNode 和嵌入 Worklet 的生命周期。
        timed_worker->reset();
        while (!timed_wait.completed.load(std::memory_order_acquire)) {
            if (hal::Clock::instance().now() >= deadline) {
                const auto stats = smp::stats(cpu::CpuId{0});
                const auto root  = kernel::timer::bsp_hrtimers().root_deadline();
                kernel::log::info(
                    "remote timer diag: posted={}, timer_ipi={}, pending={:#x}, "
                    "root_armed={}, root_when={}, worker_state={}, generation={}",
                    stats.posted, stats.timer_deadlines, smp::pending_reasons(cpu::CpuId{0}),
                    root.armed, root.when.to_nanoseconds(),
                    static_cast<u32_t>(timed_wait.completed_cpu.load(std::memory_order_acquire)),
                    0U);
                kernel::test::fail("BSP precision timer 未完成 AP timed wait");
            }
            scheduler::yield();
        }
        const auto timer_after = smp::stats(cpu::CpuId{0});
        kernel::test::require(
            timed_wait.completed_cpu.load(std::memory_order_acquire) == target.value,
            "AP timed wait 未在固定 target CPU 恢复");
        kernel::test::require(timed_wait.result.load(std::memory_order_acquire) ==
                                  static_cast<u32_t>(task::TimedWaitResult::TIMEOUT),
                              "AP timed wait 未返回 timeout");
        kernel::test::require(timer_after.timer_deadlines > timer_before.timer_deadlines,
                              "AP timed wait 未请求 BSP 本地 deadline 刷新");
        for (size_t yield_count = 0; yield_count < 8; ++yield_count) scheduler::yield();

        RemoteShootdownState shootdown_state{};
        const auto bsp_tlb_before = smp::stats(cpu::CpuId{0});
        auto shootdown_worker =
            task::Thread::create_kernel(task::kernel_proc(), shootdown_entry, &shootdown_state);
        if (!shootdown_worker ||
            !scheduler::attach(**shootdown_worker, scheduler::Placement::Pinned(target)))
            kernel::test::fail("无法创建 remote shootdown worker");
        deadline = hal::Clock::instance().now() + 300_ms;
        await_remote(shootdown_state.completed, deadline, "AP 发起的 TLB shootdown 未完成");
        const auto bsp_tlb_after = smp::stats(cpu::CpuId{0});
        kernel::test::require(shootdown_state.cpu.load(std::memory_order_acquire) == target.value,
                              "TLB shootdown worker 未在目标 AP 执行");
        kernel::test::require(shootdown_state.generation.load(std::memory_order_acquire) != 0,
                              "AP TLB shootdown 未发布 generation");
        kernel::test::require(bsp_tlb_after.tlb_shootdowns > bsp_tlb_before.tlb_shootdowns,
                              "AP TLB shootdown 未收到 BSP 远端 acknowledgement IPI");
        while (!(*shootdown_worker)->exited()) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("remote_shootdown.exit");
                kernel::test::fail("remote shootdown worker 未退出");
            }
            scheduler::yield();
        }
        shootdown_worker->reset();

        // 这两个 kernel Thread 共用一个用户 Process，因此每个目标 CPU 在首次 dispatch 时都要
        // 激活同一个 UserVm；测试不进入用户态，避免把 usrboot 生命周期混入 scheduler 验证。
        auto addr_space = task::AddrSpace::create();
        auto process    = task::Process::create();
        auto cspace     = cap::CSpace::create();
        if (!addr_space || !process || !cspace || !(*process)->set_addr_space(**addr_space) ||
            !(*process)->set_cspace(**cspace) || !(*process)->submit())
            kernel::test::fail("同一 Process 地址空间测试初始化失败");

        AddressSpaceSwitchState switch_state{};
        cap::KObjectRef<task::Thread> switch_workers[cpu::MAX_CPUS]{};
        u64_t expected_mask = 0;
        online.for_each([&](cpu::CpuId id) noexcept {
            expected_mask |= u64_t{1} << id.value;
            auto worker =
                task::Thread::create_kernel_for(**process, addr_switch_entry, &switch_state);
            if (!worker || !scheduler::attach(**worker, scheduler::Placement::Pinned(id)))
                kernel::test::fail("同一 Process 跨 CPU worker 发布失败");
            switch_workers[id.value] = std::move(*worker);
        });
        deadline = hal::Clock::instance().now() + 500_ms;
        while (switch_state.completed_mask.load(std::memory_order_acquire) != expected_mask) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("same_process.complete");
                kernel::test::fail("同一 Process 跨 CPU worker 未全部运行");
            }
            scheduler::yield();
        }
        kernel::test::require(
            switch_state.active_mask.load(std::memory_order_acquire) == expected_mask,
            "同一 Process 的每个 CPU 未完成地址空间激活");
        online.for_each([&](cpu::CpuId id) noexcept {
            while (!switch_workers[id.value]->exited()) {
                if (hal::Clock::instance().now() >= deadline) {
                    log_sched_watchdog("same_process.exit");
                    kernel::test::fail("同一 Process 跨 CPU worker 未退出");
                }
                scheduler::yield();
            }
            switch_workers[id.value].reset();
        });
        process->reset();
        addr_space->reset();
        if (!interrupts_were_enabled)
            hal::cli();
    }

    void run_smp_threads(Context &) noexcept {
        const auto online         = cpu::topology().snapshot().online;
        const u64_t expected_mask = [&]() noexcept {
            u64_t mask = 0;
            online.for_each([&](cpu::CpuId id) noexcept { mask |= u64_t{1} << id.value; });
            return mask;
        }();

        PinnedWorkState pinned_state{};
        cap::KObjectRef<task::Thread> pinned[2 * cpu::MAX_CPUS]{};
        online.for_each([&](cpu::CpuId id) noexcept {
            for (size_t worker_index = 0; worker_index < 2; ++worker_index) {
                auto worker = task::Thread::create_kernel(task::kernel_proc(), pinned_work_entry,
                                                          &pinned_state);
                if (!worker || !scheduler::attach(**worker, scheduler::Placement::Pinned(id)))
                    kernel::test::fail("pinned work worker 发布失败");
                pinned[2 * id.value + worker_index] = std::move(*worker);
            }
        });

        const auto deadline                  = hal::Clock::instance().now() + 2_s;
        const size_t expected_pinned_workers = 2 * online.count();
        while (pinned_state.completed_count.load(std::memory_order_acquire) !=
               expected_pinned_workers)
        {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("work_threads.pinned_complete");
                kernel::test::fail("pinned work worker 未全部完成");
            }
            scheduler::yield();
        }
        {
            auto counter = pinned_state.counter.lock();
            kernel::test::require(counter->value == expected_pinned_workers * 256,
                                  "synchronized counter 总计数错误");
            online.for_each([&](cpu::CpuId id) noexcept {
                kernel::test::require(counter->by_cpu[id.value] == 512,
                                      "pinned worker CPU contribution 错误");
            });
            kernel::test::require(
                pinned_state.observed_mask.load(std::memory_order_acquire) == expected_mask,
                "pinned worker CPU identity 集合错误");
        }
        online.for_each([&](cpu::CpuId id) noexcept {
            for (size_t worker_index = 0; worker_index < 2; ++worker_index) {
                auto &worker = pinned[2 * id.value + worker_index];
                while (!worker->exited()) {
                    if (hal::Clock::instance().now() >= deadline) {
                        log_sched_watchdog("work_threads.pinned_exit");
                        kernel::test::fail("pinned work worker 未退出");
                    }
                    scheduler::yield();
                }
                worker.reset();
            }
        });

        AnyWorkState any_state{};
        cap::KObjectRef<task::Thread> any_workers[cpu::MAX_CPUS]{};
        size_t any_index = 0;
        online.for_each([&](cpu::CpuId) noexcept {
            auto worker =
                task::Thread::create_kernel(task::kernel_proc(), any_work_entry, &any_state);
            if (!worker || !scheduler::attach(**worker))
                kernel::test::fail("any placement worker 发布失败");
            any_workers[any_index++] = std::move(*worker);
        });
        while (any_state.started_count.load(std::memory_order_acquire) != online.count()) {
            if (hal::Clock::instance().now() >= deadline) {
                log_sched_watchdog("work_threads.any_start");
                kernel::test::fail("any placement worker 未全部启动");
            }
            scheduler::yield();
        }
        if (online.count() > 1)
            kernel::test::require(
                (any_state.started_mask.load(std::memory_order_acquire) & ~u64_t{1}) != 0,
                "Placement::Any 在多 CPU 上始终只选择 BSP");
        any_state.release.store(true, std::memory_order_release);
        for (size_t index = 0; index < online.count(); ++index) {
            while (!any_workers[index]->exited()) {
                if (hal::Clock::instance().now() >= deadline) {
                    log_sched_watchdog("work_threads.any_exit");
                    kernel::test::fail("any placement worker 未退出");
                }
                scheduler::yield();
            }
            any_workers[index].reset();
        }
        const auto snapshot    = cpu::topology().snapshot();
        size_t failed_count    = 0;
        size_t abandoned_count = 0;
        snapshot.possible.for_each([&](cpu::CpuId id) noexcept {
            if (cpu::topology().state(id) == cpu::CpuState::FAILED)
                ++failed_count;
            if (smp::ap_manager().state(id) == boot::smp::StartState::ABANDONED)
                ++abandoned_count;
        });
        kernel::log::info("SMP selftest summary: possible={}, started={}, online={}",
                          snapshot.possible.count(), snapshot.started.count(),
                          snapshot.online.count());
        kernel::log::info("  failed={}, abandoned={}", failed_count, abandoned_count);
        snapshot.online.for_each([](cpu::CpuId id) noexcept {
            const auto stats           = smp::stats(id);
            const auto scheduler_state = scheduler::for_cpu(id).debug_state();
            kernel::log::info(
                "  cpu={} dispatch={} preemption={} ipi_dispatch={} resched_ipi={} tlb_ack={} "
                "timer_ipi={}",
                id.value, scheduler_state.context_switch_count, scheduler_state.preemption_count,
                stats.dispatches, stats.reschedules, stats.tlb_shootdowns, stats.timer_deadlines);
        });
    }
}  // namespace kernel::test::cases
