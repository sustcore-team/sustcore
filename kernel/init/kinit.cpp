/**
 * @file kinit.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kinit 永久内核线程、Scheduler、BSP WorkQueue 与周期 timer 启动编排。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <arch/timer.h>
#include <async/work_queue.h>
#include <init/kinit.h>
#include <log.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#ifdef CONFIG_KERNEL_SELFTEST
#include <test/framework.h>
#endif
#include <timer/deadline.h>
#include <timer/timer_engine.h>

namespace init {
    namespace {
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
                auto &engine = kernel::timer::bsp_timer_engine();
                if (pending() || engine.state(timer_) != kernel::timer::PrecisionTimerState::IDLE)
                    kernel::log::panic("重复启动 periodic timer Worklet");
                arm_next();
            }

        private:
            void run() noexcept override {
                auto &engine = kernel::timer::bsp_timer_engine();
                engine.retire(timer_);
                engine.reset(timer_);

                kernel::log::info("timer worklet ran!");
                arm_next();
            }

            void arm_next() noexcept {
                const auto deadline = kernel::timer::saturated_deadline_after(
                    hal::CpuClock::instance().current_time(), PERIOD);
                if (auto armed = kernel::timer::bsp_timer_engine().arm(timer_, deadline, *this);
                    !armed)
                    kernel::log::panic("periodic timer Worklet 重新 arm 失败: {}", armed.error());
            }

            static constexpr auto PERIOD = 1_s;

            kernel::timer::PrecisionTimerNode timer_{};
        };

        constinit PeriodicTimerWorklet periodic_timer_worklet;

        void start_bsp_work_queue() noexcept {
            auto &queue = kernel::async::bsp_work_queue();
            if (auto started = queue.start(task::kernel_process()); !started)
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
    }  // namespace

    [[noreturn]] void run_kinit() noexcept {
        auto initialized_process = task::initialize_kernel_process();
        if (!initialized_process)
            kernel::log::panic("无法初始化 kernel_process: {}", initialized_process.error());
        auto kinit       = task::Thread::adopt_current(task::kernel_process());
        auto initialized = scheduler::instance().initialize(kinit);
        if (!initialized)
            kernel::log::panic("无法初始化 Scheduler: {}", initialized.error());
        if (auto installed = scheduler::instance().install_preemption_deadline_sink(
                kernel::timer::bsp_deadline_state().preemption_sink());
            !installed)
            kernel::log::panic("无法安装 BSP scheduler deadline sink: {}", installed.error());

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::POST_SCHEDULER_INITIALIZATION,
                                {.current_thread = &kinit});
#endif

        start_bsp_work_queue();

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::POST_WORK_QUEUE_INITIALIZATION,
                                {.current_thread = &kinit});
#endif

        auto usrboot = start_usrboot();
        if (!usrboot)
            kernel::log::panic("usrboot 启动失败: {}", usrboot.error());

#ifdef CONFIG_KERNEL_SELFTEST
        kernel::test::run_phase(kernel::test::Phase::PRE_IDLE, {.current_thread = &kinit});
#endif

        periodic_timer_worklet.start();

        // bootstrap Thread 在完成初始化职责后成为本 CPU 的 idle；它不进入 ready queue，
        // 普通 Thread 唤醒或 timer tick 会在 trap-return 路径将它换出。
        scheduler::instance().become_idle();
        scheduler::yield();
        hal::enable_interrupts();
        for (;;) hal::wait_for_interrupt();
    }
}  // namespace init
