/**
 * @file timer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Precision timer、timed wait 与周期 Worklet selftest。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <arch/timer.h>
#include <async/worklet.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <test/cases.h>
#include <timer/deadline.h>
#include <timer/hrtimer.h>

#include <atomic>
#include <cstddef>
#include <utility>

namespace kernel::test::cases {
    namespace {
        class PeriodicTimerProbe final {
        private:
            class CompletionWorklet final : public kernel::async::Worklet {
            public:
                explicit constexpr CompletionWorklet(PeriodicTimerProbe &owner) noexcept
                    : owner_(&owner) {}

            private:
                void run() noexcept override {
                    owner_->complete();
                }

                PeriodicTimerProbe *owner_ = nullptr;
            };

        public:
            constexpr PeriodicTimerProbe() noexcept : completion_(*this) {}

            void run() noexcept {
                auto &engine = kernel::timer::bsp_hrtimers();
                if (running_.load(std::memory_order_acquire) || completion_.pending() ||
                    engine.state(timer_) != kernel::timer::HRTState::IDLE)
                    kernel::test::fail("重复启动 periodic timer probe");

                dispatch_count_ = 0;
                running_.store(true, std::memory_order_release);
                next_deadline_ =
                    kernel::timer::deadline_after(hal::Clock::instance().now(), PERIOD);
                arm_next();
                while (running_.load(std::memory_order_acquire)) scheduler::yield();
                kernel::test::require(dispatch_count_ == EXPECTED_DISPATCH_COUNT,
                                      "periodic timer probe dispatch 次数错误");
            }

        private:
            void complete() noexcept {
                if (!running_.load(std::memory_order_acquire))
                    kernel::test::fail("periodic timer probe 在启动前运行");

                auto &engine = kernel::timer::bsp_hrtimers();
                engine.retire(timer_);
                engine.reset(timer_);
                ++dispatch_count_;

                if (dispatch_count_ == EXPECTED_DISPATCH_COUNT) {
                    running_.store(false, std::memory_order_release);
                    return;
                }

                const auto now = hal::Clock::instance().now();
                next_deadline_ = kernel::timer::deadline_after(now, PERIOD);
                arm_next();
            }

            void arm_next() noexcept {
                auto armed = kernel::timer::bsp_hrtimers().arm(timer_, next_deadline_, completion_);
                if (!armed)
                    kernel::test::fail("periodic timer probe 重新 arm 失败");
            }

            static constexpr auto PERIOD                    = 2_ms;
            static constexpr size_t EXPECTED_DISPATCH_COUNT = 2;

            kernel::timer::HrTimer timer_{};
            CompletionWorklet completion_;
            units::time next_deadline_{};
            size_t dispatch_count_ = 0;
            std::atomic<bool> running_{false};
        };

        static_assert(std::atomic<bool>::is_always_lock_free);

        struct TimedWaitCase final {
            units::time deadline{};
            task::TimedWaitResult result = task::TimedWaitResult::NONE;
            bool completed               = false;
        };

        struct RepeatedTimedWaitCase final {
            units::time first_deadline{};
            units::time second_deadline{};
            task::TimedWaitResult first_result  = task::TimedWaitResult::NONE;
            task::TimedWaitResult second_result = task::TimedWaitResult::NONE;
            bool first_completed                = false;
            bool completed                      = false;
        };

        constinit PeriodicTimerProbe periodic_timer_probe;

        void timed_wait_entry(void *opaque) noexcept {
            auto *state = static_cast<TimedWaitCase *>(opaque);
            if (state == nullptr)
                kernel::test::fail("timed-wait 用例没有状态");
            auto result = scheduler::local().current_thread().wait_until(state->deadline);
            if (!result)
                kernel::test::fail("timed-wait 执行失败");
            state->result    = *result;
            state->completed = true;
        }

        void repeat_wait_entry(void *opaque) noexcept {
            auto *state = static_cast<RepeatedTimedWaitCase *>(opaque);
            if (state == nullptr)
                kernel::test::fail("repeated timed-wait 用例没有状态");
            auto &current = scheduler::local().current_thread();
            auto first    = current.wait_until(state->first_deadline);
            if (!first)
                kernel::test::fail("首次 repeated timed wait 失败");
            state->first_result    = *first;
            state->first_completed = true;
            auto second            = current.wait_until(state->second_deadline);
            if (!second)
                kernel::test::fail("第二次 repeated timed wait 失败");
            state->second_result = *second;
            state->completed     = true;
        }

        void deadline_racer(void *) noexcept {}

        [[nodiscard]] cap::KObjectRef<task::Thread> start_wait_case(TimedWaitCase &state) noexcept {
            auto thread =
                task::Thread::create_kernel(task::kernel_proc(), timed_wait_entry, &state);
            if (!thread)
                kernel::test::fail("无法创建 timed-wait Thread");
            if (auto attached = scheduler::attach(**thread); !attached)
                kernel::test::fail("无法发布 timed-wait Thread");
            scheduler::yield();
            if ((*thread)->state() != task::ThreadState::BLOCKED && !(*thread)->exited())
                kernel::test::fail("timed-wait Thread 未阻塞并发布 generation");
            if ((*thread)->state() == task::ThreadState::BLOCKED && (*thread)->wait_gen() == 0)
                kernel::test::fail("已阻塞 timed-wait Thread 没有 generation");
            return std::move(*thread);
        }

        void await_wait_exit(task::Thread &thread) noexcept {
            while (!thread.exited()) scheduler::yield();
            if (!thread.wait_idle())
                kernel::test::fail("timed-wait Thread 退出时仍借用 timer 资源");
        }
    }  // namespace

    void run_precise_timer(Context &) noexcept {
        auto &clock                        = hal::Clock::instance();
        auto &engine                       = kernel::timer::bsp_hrtimers();
        auto &deadlines                    = kernel::timer::bsp_deadline_mux();
        const bool interrupts_were_enabled = hal::irq_enabled();
        if (!interrupts_were_enabled)
            hal::sti();

        TimedWaitCase timeout_case{
            .deadline = kernel::timer::deadline_after(clock.now(), 2_ms),
        };
        auto timeout_thread = start_wait_case(timeout_case);
        await_wait_exit(*timeout_thread);
        kernel::test::require(
            timeout_case.completed && timeout_case.result == task::TimedWaitResult::TIMEOUT,
            "timed wait 未按 timeout 完成");
        timeout_thread.reset();

        const auto now = clock.now();
        TimedWaitCase past_case{
            .deadline = now == units::time{} ? now : now - 1_ns,
        };
        auto past_thread = start_wait_case(past_case);
        await_wait_exit(*past_thread);
        kernel::test::require(past_case.result == task::TimedWaitResult::TIMEOUT,
                              "过期 timed wait 未异步超时");
        past_thread.reset();

        TimedWaitCase wake_case{
            .deadline = kernel::timer::deadline_after(clock.now(), 1_s),
        };
        auto wake_thread            = start_wait_case(wake_case);
        const u64_t wake_generation = wake_thread->wait_gen();
        kernel::test::require(
            !scheduler::local().detach(*wake_thread, scheduler::DetachReason::SUSPEND) &&
                !scheduler::local().detach(*wake_thread, scheduler::DetachReason::TERMINATE),
            "scheduler detach 了活跃 timed wait 中的 Thread");

        TimedWaitCase nonroot_case{
            .deadline = kernel::timer::deadline_after(clock.now(), 2_s),
        };
        auto nonroot_thread            = start_wait_case(nonroot_case);
        const u64_t nonroot_generation = nonroot_thread->wait_gen();
        kernel::test::require(nonroot_thread->cancel_wait(nonroot_generation),
                              "non-root timed-wait cancel 丢失");
        await_wait_exit(*nonroot_thread);
        kernel::test::require(nonroot_case.result == task::TimedWaitResult::CANCELLED &&
                                  engine.root_deadline().armed &&
                                  engine.root_deadline().when == wake_case.deadline,
                              "non-root timer cancel 改变了存活 root");
        nonroot_thread.reset();

        auto competitor = task::Thread::create_kernel(task::kernel_proc(), deadline_racer);
        if (!competitor || !scheduler::attach(**competitor))
            kernel::test::fail("无法创建 timer/RR deadline 竞争者");
        const auto timer_source      = deadlines.timer_deadline();
        const auto preemption_source = deadlines.preempt_deadline();
        const auto programmed        = deadlines.armed_deadline();
        kernel::test::require(timer_source.armed && preemption_source.armed && programmed.armed &&
                                  programmed.when == (timer_source.when <= preemption_source.when
                                                          ? timer_source.when
                                                          : preemption_source.when),
                              "timer root 与 RR deadline 未按最小值合并");
        scheduler::yield();
        kernel::test::require((*competitor)->exited(), "timer/RR deadline 竞争者未退出");
        competitor->reset();

        kernel::test::require(!wake_thread->wake_wait(wake_generation - 1) &&
                                  wake_thread->wake_wait(wake_generation) &&
                                  !wake_thread->wake_wait(wake_generation),
                              "timed-wait wake 未准确选择一个胜者");
        await_wait_exit(*wake_thread);
        kernel::test::require(wake_case.result == task::TimedWaitResult::WOKEN,
                              "timed-wait wake 返回了错误结果");
        wake_thread.reset();

        RepeatedTimedWaitCase repeated{
            .first_deadline  = kernel::timer::deadline_after(clock.now(), 1_s),
            .second_deadline = kernel::timer::deadline_after(clock.now(), 1_s),
        };
        auto repeated_thread =
            task::Thread::create_kernel(task::kernel_proc(), repeat_wait_entry, &repeated);
        if (!repeated_thread || !scheduler::attach(**repeated_thread))
            kernel::test::fail("无法创建 repeated timed-wait Thread");
        scheduler::yield();
        const u64_t first_generation = (*repeated_thread)->wait_gen();
        kernel::test::require((*repeated_thread)->wake_wait(first_generation),
                              "repeated timed-wait 首次 wake 丢失");
        while (!repeated.first_completed || (*repeated_thread)->wait_gen() == first_generation)
            scheduler::yield();
        const u64_t second_generation = (*repeated_thread)->wait_gen();
        kernel::test::require(!(*repeated_thread)->cancel_wait(first_generation) &&
                                  (*repeated_thread)->cancel_wait(second_generation) &&
                                  !(*repeated_thread)->cancel_wait(second_generation),
                              "timed-wait stale/duplicate cancel 语义错误");
        await_wait_exit(**repeated_thread);
        kernel::test::require(repeated.completed &&
                                  repeated.first_result == task::TimedWaitResult::WOKEN &&
                                  repeated.second_result == task::TimedWaitResult::CANCELLED,
                              "repeated timed-wait 结果不一致");
        repeated_thread->reset();

        const auto shared_deadline   = kernel::timer::deadline_after(clock.now(), 2_ms);
        TimedWaitCase equal_cases[2] = {
            TimedWaitCase{.deadline = shared_deadline},
            TimedWaitCase{.deadline = shared_deadline},
        };
        auto equal_first  = start_wait_case(equal_cases[0]);
        auto equal_second = start_wait_case(equal_cases[1]);
        await_wait_exit(*equal_first);
        await_wait_exit(*equal_second);
        kernel::test::require(equal_cases[0].result == task::TimedWaitResult::TIMEOUT &&
                                  equal_cases[1].result == task::TimedWaitResult::TIMEOUT,
                              "相同 deadline 的 timer 未一起 drain");
        equal_first.reset();
        equal_second.reset();

        kernel::test::require(engine.size() == 0 && !engine.root_deadline().armed &&
                                  !deadlines.timer_deadline().armed,
                              "空 precision timer heap 未停用 deadline source");
        if (!interrupts_were_enabled)
            hal::cli();
    }

    void run_timer_probe(Context &) noexcept {
        const bool interrupts_were_enabled = hal::irq_enabled();
        if (!interrupts_were_enabled)
            hal::sti();
        periodic_timer_probe.run();
        if (!interrupts_were_enabled)
            hal::cli();
    }
}  // namespace kernel::test::cases
