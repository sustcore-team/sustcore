/**
 * @file kinit.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kinit 永久内核线程、Scheduler、BSP WorkQueue 与周期 timer 启动编排。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/cpu.h>
#include <arch/interrupt.h>
#include <arch/timer.h>
#include <async/queue.h>
#include <boot/smp.h>
#include <cpu/local.h>
#include <cpu/topology.h>
#include <init/kinit.h>
#include <init/milestones.h>
#include <log.h>
#include <memory/virtual/kernel/vm.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <smp/ap.h>
#ifdef CONFIG_KERNEL_SELFTEST
#include <test/framework.h>
#endif
#include <timer/deadline.h>
#include <timer/hrtimer.h>

namespace init {
    namespace detail::startup {
        /**
         * @brief 永久宿主的周期 timer Worklet，每秒在普通 worker 上输出一次日志。
         *
         * run() 先 retire/reset 当前 timer node，再以当前时刻为基准重新 arm，避免工作
         * 延迟时累积补发。对象具有与内核相同的生命期，成功重新 arm 后不销毁宿主。
         */
        class PeriodicTimerWorklet final : public kernel::async::Worklet {
        public:
            constexpr PeriodicTimerWorklet() noexcept = default;

            void start() noexcept {
                auto &engine = kernel::timer::bsp_hrtimers();
                if (pending() || engine.state(timer_) != kernel::timer::HRTState::IDLE)
                    kernel::log::panic("重复启动 periodic timer Worklet");
                arm_next();
            }

        private:
            void run() noexcept override {
                auto &engine = kernel::timer::bsp_hrtimers();
                engine.retire(timer_);
                engine.reset(timer_);

                kernel::log::info("timer worklet ran!");
                arm_next();
            }

            void arm_next() noexcept {
                const auto deadline =
                    kernel::timer::deadline_after(hal::Clock::instance().now(), PERIOD);
                if (auto armed = kernel::timer::bsp_hrtimers().arm(timer_, deadline, *this); !armed)
                    kernel::log::panic("periodic timer Worklet 重新 arm 失败: {}", armed.error());
            }

            static constexpr auto PERIOD = 1_s;

            kernel::timer::HrTimer timer_{};
        };

        constinit PeriodicTimerWorklet periodic_timer_worklet;
        constinit task::KernelStack ap_stacks[cpu::MAX_CPUS]{};

        // 未启动的 AP 不能无限占用 BSP scheduler；该预算仅覆盖 firmware handoff，超过后
        // commit 前降级为 ABANDONED。真实时间压力与跨 CPU 调度测试留给后续阶段。
        constexpr auto AP_READY_TIMEOUT = 200_ms;

        void start_bsp_queue() noexcept {
            auto &queue = kernel::async::bsp_work_queue();
            if (auto started =
                    queue.start(task::kernel_proc(), scheduler::Placement::Pinned(cpu::CpuId{0}));
                !started)
                kernel::log::panic("BSP WorkQueue worker 创建失败: {}", started.error());

            // 永久 worker 初始无工作，应离开 ready queue；timer 后续只需 post 到该固定 queue。
            auto *worker = queue.worker();
            while (worker != nullptr && worker->state() != task::ThreadState::BLOCKED &&
                   !worker->exited())
                scheduler::yield();
            if (worker == nullptr || worker->state() != task::ThreadState::BLOCKED ||
                queue.pending_count() != 0 || !queue.accepting())
                kernel::log::panic("BSP WorkQueue worker 启动后未 park");
            if (queue.shutdown() || !queue.accepting() || queue.worker() != worker)
                kernel::log::panic("永久 BSP WorkQueue 接受了 shutdown");
            kernel::log::info("BSP WorkQueue worker 已启动并 park");
        }

        void await_ap_ready() noexcept {
            const auto possible = cpu::topology().snapshot().possible.without(cpu::CpuId{0});
            const auto deadline = hal::Clock::instance().now() + AP_READY_TIMEOUT;
            while (hal::Clock::instance().now() < deadline) {
                bool waiting = false;
                possible.for_each([&waiting](cpu::CpuId id) noexcept {
                    const auto state  = smp::ap_manager().state(id);
                    waiting          |= state == boot::smp::StartState::STARTING ||
                               state == boot::smp::StartState::EARLY_ONLINE;
                });
                if (!waiting)
                    return;
                hal::cpu_relax();
            }

            possible.for_each([](cpu::CpuId id) noexcept {
                const auto gen = smp::ap_manager().generation(id);
                if (gen != 0)
                    static_cast<void>(smp::ap_manager().abandon(id, gen));
            });
        }

        void await_ap_online() noexcept {
            const auto committed = smp::ap_manager().committed_set().without(cpu::CpuId{0});
            const auto deadline  = hal::Clock::instance().now() + AP_READY_TIMEOUT;
            while (hal::Clock::instance().now() < deadline) {
                bool complete = true;
                committed.for_each([&complete](cpu::CpuId id) noexcept {
                    const auto *boot_res  = smp::ap_manager().boot_res(id);
                    complete             &= boot_res != nullptr &&
                                boot_res->online_ack.load(std::memory_order_acquire) &&
                                smp::ap_manager().state(id) == boot::smp::StartState::ONLINE &&
                                cpu::topology().state(id) == cpu::CpuState::ONLINE;
                });
                if (complete)
                    return;
                hal::cpu_relax();
            }
            committed.for_each([](cpu::CpuId id) noexcept {
                kernel::log::panic("AP 在 online commit 后未确认: cpu={}, state={}", id.value,
                                   static_cast<u32_t>(smp::ap_manager().state(id)));
            });
        }

        void smp_init_entry(void *) noexcept {
            const auto possible = cpu::topology().snapshot().possible.without(cpu::CpuId{0});
            possible.for_each([](cpu::CpuId id) noexcept {
                auto stack = task::KernelStack::create();
                if (!stack) {
                    kernel::log::warn("无法为 AP 分配启动栈，降级跳过: cpu={}, error={}", id.value,
                                      stack.error());
                    return;
                }
                ap_stacks[id.value] = std::move(*stack);
                auto *storage       = cpu::try_slot(id);
                if (storage == nullptr)
                    kernel::log::panic("AP CpuSlot 缺失: cpu={}", id.value);
                auto prepared = smp::ap_manager().prepare(
                    id, cpu::topology().hw_id(id), memory::kernel_vm().root(),
                    ap_stacks[id.value].top(), reinterpret_cast<addr_t>(&storage->local),
                    reinterpret_cast<addr_t>(&smp::ap_main));
                if (!prepared) {
                    kernel::log::warn("AP 启动参数准备失败，降级跳过: cpu={}, error={}", id.value,
                                      prepared.error());
                    // prepare 未把参数交付给固件，启动栈仍由 BSP 独占，可确定回收。
                    ap_stacks[id.value] = task::KernelStack{};
                    return;
                }
                auto started = smp::ap_manager().start(id, boot::smp::start_ap);
                if (!started) {
                    kernel::log::warn("AP 启动请求失败，降级跳过: cpu={}, error={}", id.value,
                                      started.error());
                    // starter 以 expected 返回同步固件拒绝；AP 未交付启动，因此可以安全回收栈。
                    ap_stacks[id.value] = task::KernelStack{};
                }
            });

            await_ap_ready();
            if (!smp::ap_manager().commit_ready_set())
                kernel::log::panic("SMP online 集合被重复提交");
            await_ap_online();
            const auto snapshot = cpu::topology().snapshot();
            kernel::log::info(
                "SMP online 集合已提交: possible={}, started={}, online={}, cpu_set={}",
                snapshot.possible.count(), snapshot.started.count(), snapshot.online.count(),
                snapshot.online);
        }

        void run_smp_init() noexcept {
            auto thread = task::Thread::create_kernel(task::kernel_proc(), smp_init_entry);
            if (!thread)
                kernel::log::panic("无法创建 smp-init Thread: {}", thread.error());
            auto attached =
                scheduler::attach(**thread, scheduler::Placement::Pinned(cpu::CpuId{0}));
            if (!attached)
                kernel::log::panic("无法固定 smp-init Thread 到 BSP: {}", attached.error());
            while (!(*thread)->exited()) scheduler::yield();
        }
    }  // namespace detail::startup

    [[noreturn]] void run_kinit() noexcept {
        auto initialized_process = task::init_kernel_proc();
        if (!initialized_process)
            kernel::log::panic("无法初始化 kernel_proc: {}", initialized_process.error());
        auto kinit       = task::Thread::adopt_current(task::kernel_proc());
        auto initialized = scheduler::local().initialize(kinit);
        if (!initialized)
            kernel::log::panic("无法初始化 Scheduler: {}", initialized.error());
        if (auto installed = scheduler::local().set_preempt_sink(
                kernel::timer::bsp_deadline_mux().preemption_sink());
            !installed)
            kernel::log::panic("无法安装 BSP scheduler deadline sink: {}", installed.error());
        init::advance(init::Milestone::TIMER_READY, init::Milestone::SCHEDULER_READY);

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::POST_SCHED_INIT, {.current_thread = &kinit});
#endif

        detail::startup::start_bsp_queue();

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::POST_WORK_QUEUE_INITIALIZATION,
                                {.current_thread = &kinit});
#endif

        // 保持既有 scheduler/timer selftest 的基线不被固件 AP handoff 的有界等待扰动；
        // smp-init 仍是独立固定 BSP Thread，且在任何用户 Thread 发布前完成 online commit。
        detail::startup::run_smp_init();
        init::advance(init::Milestone::SCHEDULER_READY, init::Milestone::SMP_READY);

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::POST_SMP_INIT, {.current_thread = &kinit});
#endif

        auto usrboot = start_usrboot();
        if (!usrboot)
            kernel::log::panic("usrboot 启动失败: {}", usrboot.error());

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::PRE_IDLE, {.current_thread = &kinit});
#endif

        detail::startup::periodic_timer_worklet.start();

        // bootstrap Thread 在完成初始化职责后成为本 CPU 的 idle；它不进入 ready queue，
        // 普通 Thread 唤醒或 timer tick 会在 trap-return 路径将它换出。
        scheduler::local().become_idle();
        scheduler::yield();
        hal::sti();
        while (true) hal::wfi();
    }
}  // namespace init
