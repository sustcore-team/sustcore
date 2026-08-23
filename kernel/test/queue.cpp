/**
 * @file queue.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief WorkQueue 发布、调度、公平性与关闭 selftest。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <arch/timer.h>
#include <async/queue.h>
#include <cpu/topology.h>
#include <log.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <test/cases.h>
#include <timer/hrtimer.h>

#include <atomic>
#include <cstddef>
#include <new>

namespace kernel::test::cases {
    namespace {
        struct FairnessState final {
            kernel::async::WorkQueue *queue = nullptr;
            u64_t *counter                  = nullptr;
            bool observer_ran               = false;
        };

        class CounterWorklet final : public kernel::async::Worklet {
        public:
            constexpr CounterWorklet() noexcept = default;
            explicit constexpr CounterWorklet(u64_t &counter, u64_t increment = 1) noexcept
                : counter_(&counter), increment_(increment) {}

            void setup(u64_t &counter, u64_t increment = 1) noexcept {
                if (pending())
                    kernel::test::fail("不能重新配置 pending CounterWorklet");
                counter_   = &counter;
                increment_ = increment;
            }

        private:
            void run() noexcept override {
                auto *counter        = counter_;
                const auto increment = increment_;
                if (counter == nullptr)
                    kernel::test::fail("CounterWorklet 未绑定 counter");
                *counter += increment;
            }

            u64_t *counter_  = nullptr;
            u64_t increment_ = 1;
        };

        class RepostWorklet final : public kernel::async::Worklet {
        public:
            constexpr RepostWorklet(kernel::async::WorkQueue &queue, u64_t &dispatch_count,
                                    u64_t &repost_count, size_t remaining_reposts) noexcept
                : queue_(&queue),
                  dispatch_count_(&dispatch_count),
                  repost_count_(&repost_count),
                  remaining_reposts_(remaining_reposts) {}

        private:
            void run() noexcept override {
                if (pending())
                    kernel::test::fail("self-repost Worklet 进入 run 时仍为 pending");

                auto *queue          = queue_;
                auto *dispatch_count = dispatch_count_;
                auto *repost_count   = repost_count_;
                if (queue == nullptr || dispatch_count == nullptr || repost_count == nullptr)
                    kernel::test::fail("self-repost Worklet 状态不完整");

                ++*dispatch_count;
                if (remaining_reposts_ == 0)
                    return;

                --remaining_reposts_;
                if (!queue->try_post(*this))
                    kernel::test::fail("WorkQueue 拒绝 IDLE self-repost Worklet");
                ++*repost_count;

                // repost 成功后 WorkQueue 再次借用本对象；本次 run 不销毁宿主。
            }

            kernel::async::WorkQueue *queue_ = nullptr;
            u64_t *dispatch_count_           = nullptr;
            u64_t *repost_count_             = nullptr;
            size_t remaining_reposts_        = 0;
        };

        struct TailDispatchState final {
            u64_t dispatched = 0;
            u64_t destroyed  = 0;
        };

        class TailDispatchHost;

        class DestroyWorklet final : public kernel::async::Worklet {
        public:
            explicit constexpr DestroyWorklet(TailDispatchHost &owner) noexcept : owner_(&owner) {}

        private:
            void run() noexcept override;

            TailDispatchHost *owner_ = nullptr;
        };

        class TailDispatchHost final {
        public:
            explicit constexpr TailDispatchHost(TailDispatchState &state) noexcept
                : state_(&state), worklet_(*this) {}

            ~TailDispatchHost() noexcept {
                ++state_->destroyed;
            }

            [[nodiscard]] DestroyWorklet &worklet() noexcept {
                return worklet_;
            }

        private:
            TailDispatchState *state_ = nullptr;
            DestroyWorklet worklet_;

            friend class DestroyWorklet;
        };

        void DestroyWorklet::run() noexcept {
            auto *owner = owner_;
            owner_      = nullptr;
            if (owner == nullptr)
                kernel::test::fail("tail-dispatch Worklet 没有 owner");
            auto *state = owner->state_;
            if (state == nullptr)
                kernel::test::fail("tail-dispatch host 没有结果状态");
            ++state->dispatched;

            // delete 会连带销毁本 Worklet；这是本函数及 WorkQueue 对它的最后一次访问。
            delete owner;
        }

        void fairness_observer(void *opaque) noexcept {
            auto *state = static_cast<FairnessState *>(opaque);
            if (state == nullptr || state->queue == nullptr || state->counter == nullptr)
                kernel::test::fail("WorkQueue fairness observer 状态无效");
            if (*state->counter != kernel::async::WorkQueue::BATCH_BUDGET ||
                state->queue->pending_count() != 1)
                kernel::test::fail("WorkQueue worker 未在固定 batch budget 后 yield");
            state->observer_ran = true;
        }

        constexpr size_t IRQ_WORK_ROUNDS       = 24;
        constexpr size_t IRQ_WORKLET_PER_ROUND = 8;

        struct IrqWorkQueueStressState final {
            kernel::async::WorkQueue *queue = nullptr;
            units::time deadline{};
            std::atomic<bool> abort{false};
            // 保留首个失败原因；watchdog 诊断不能为了读取 WorkQueue 状态再次获取其锁。
            std::atomic<u32_t> abort_reason{0};
            std::atomic<u64_t> active_dispatches{0};
            std::atomic<u64_t> dispatched{0};
            std::atomic<u64_t> irq_dispatches{0};
            std::atomic<size_t> producers_done{0};
            bool timer_completion_enabled = false;
            size_t rounds                 = IRQ_WORK_ROUNDS;
        };

        void abort_irq_stress(IrqWorkQueueStressState &stress, u32_t reason) noexcept {
            u32_t expected = 0;
            static_cast<void>(stress.abort_reason.compare_exchange_strong(
                expected, reason, std::memory_order_acq_rel, std::memory_order_relaxed));
            stress.abort.store(true, std::memory_order_release);
        }

        struct IrqWorkQueueProducerState;

        class IrqWorkQueueWorklet final : public kernel::async::Worklet {
        public:
            constexpr IrqWorkQueueWorklet() noexcept = default;

            void bind(IrqWorkQueueProducerState &producer) noexcept {
                producer_ = &producer;
            }

            void mark_timer_done() noexcept {
                timer_completion_ = true;
            }

        private:
            void run() noexcept override;

            IrqWorkQueueProducerState *producer_ = nullptr;
            bool timer_completion_               = false;
        };

        struct IrqWorkQueueProducerState final {
            IrqWorkQueueStressState *stress = nullptr;
            cpu::CpuId target{};
            std::atomic<u64_t> posted{0};
            std::atomic<u64_t> completed{0};
            std::atomic<u32_t> phase{0};
            IrqWorkQueueWorklet worklets[IRQ_WORKLET_PER_ROUND]{};
            kernel::timer::HrTimer timer_node{};
            IrqWorkQueueWorklet timer_worklet{};
        };

        void IrqWorkQueueWorklet::run() noexcept {
            auto *producer = producer_;
            if (producer == nullptr || producer->stress == nullptr)
                kernel::test::fail("IRQ WorkQueue Worklet 状态无效");
            if (pending())
                kernel::test::fail("IRQ WorkQueue Worklet 在 dispatch 时仍为 pending");
            auto &stress = *producer->stress;
            if (timer_completion_) {
                auto &engine = kernel::timer::bsp_hrtimers();
                if (engine.state(producer->timer_node) != kernel::timer::HRTState::POSTED)
                    abort_irq_stress(stress, 4);
                else {
                    engine.retire(producer->timer_node);
                    engine.reset(producer->timer_node);
                }
            }
            const auto previous = stress.active_dispatches.fetch_add(1, std::memory_order_acq_rel);
            if (previous != 0)
                abort_irq_stress(stress, 5);
            if (hal::irq_enabled())
                stress.irq_dispatches.fetch_add(1, std::memory_order_relaxed);
            stress.dispatched.fetch_add(1, std::memory_order_relaxed);
            stress.active_dispatches.fetch_sub(1, std::memory_order_release);
            // completion 发布在 run() 的最后一步，producer 只能在本次 dispatch 完全退出后
            // 重新借用稳定宿主中的 Worklet。
            producer->completed.fetch_add(1, std::memory_order_release);
        }

        void irq_queue_producer(void *opaque) noexcept {
            auto *producer = static_cast<IrqWorkQueueProducerState *>(opaque);
            if (producer == nullptr || producer->stress == nullptr)
                kernel::test::fail("IRQ WorkQueue producer 状态无效");
            auto &stress = *producer->stress;
            if (cpu::current_id() != producer->target)
                abort_irq_stress(stress, 2);
            if (!hal::irq_enabled())
                abort_irq_stress(stress, 3);

            for (size_t round = 0;
                 round < stress.rounds && !stress.abort.load(std::memory_order_acquire); ++round)
            {
                producer->phase.store(static_cast<u32_t>(round * 2 + 1), std::memory_order_release);
                for (auto &worklet : producer->worklets) {
                    while (!stress.abort.load(std::memory_order_acquire) &&
                           !stress.queue->try_post(worklet))
                    {
                        if (hal::Clock::instance().now() >= stress.deadline) {
                            abort_irq_stress(stress, 6);
                            break;
                        }
                        scheduler::yield();
                    }
                    if (stress.abort.load(std::memory_order_acquire))
                        break;
                    producer->posted.fetch_add(1, std::memory_order_release);
                }

                const auto expected = producer->posted.load(std::memory_order_acquire);
                producer->phase.store(static_cast<u32_t>(round * 2 + 2), std::memory_order_release);
                while (!stress.abort.load(std::memory_order_acquire) &&
                       producer->completed.load(std::memory_order_acquire) < expected)
                {
                    if (hal::Clock::instance().now() >= stress.deadline) {
                        abort_irq_stress(stress, 7);
                        break;
                    }
                    scheduler::yield();
                }
                if (stress.abort.load(std::memory_order_acquire))
                    break;
                if (!stress.timer_completion_enabled)
                    continue;
                const auto timer_deadline =
                    kernel::timer::deadline_after(hal::Clock::instance().now(), 1_ms);
                if (!kernel::timer::bsp_hrtimers().arm(producer->timer_node, timer_deadline,
                                                       producer->timer_worklet))
                {
                    abort_irq_stress(stress, 8);
                    break;
                }
                producer->posted.fetch_add(1, std::memory_order_release);
                const auto timer_expected = producer->posted.load(std::memory_order_acquire);
                while (!stress.abort.load(std::memory_order_acquire) &&
                       producer->completed.load(std::memory_order_acquire) < timer_expected)
                {
                    if (hal::Clock::instance().now() >= stress.deadline) {
                        abort_irq_stress(stress, 9);
                        break;
                    }
                    scheduler::yield();
                }
            }
            stress.producers_done.fetch_add(1, std::memory_order_release);
        }
    }  // namespace

    void run_work_queue(Context &) noexcept {
        kernel::async::WorkQueue queue;
        if (auto started = queue.start(task::kernel_proc()); !started)
            kernel::test::fail("WorkQueue selftest worker 创建失败");

        // 先让 worker 在空队列上完成两阶段 park，后续首个 post 必须从 BLOCKED 唤醒它。
        auto *worker = queue.worker();
        while (worker != nullptr && worker->state() != task::ThreadState::BLOCKED &&
               !worker->exited())
            scheduler::yield();
        kernel::test::require(worker != nullptr && worker->state() == task::ThreadState::BLOCKED,
                              "WorkQueue worker 未在空队列上 park");

        u64_t first_counter = 0;
        CounterWorklet first(first_counter);
        kernel::test::require(queue.try_post(first), "WorkQueue 拒绝首个 selftest post");
        kernel::test::require(!queue.try_post(first), "WorkQueue 接受了重复链接的 Worklet");
        kernel::test::require(first_counter == 0, "try_post 内联执行了 Worklet");
        scheduler::yield();
        kernel::test::require(first_counter == 1 && !first.pending() && queue.pending_count() == 0,
                              "WorkQueue 未准确执行首个 Worklet 一次");

        {
            kernel::async::WorkQueue alternate_queue;
            if (auto started = alternate_queue.start(task::kernel_proc()); !started)
                kernel::test::fail("无法创建 alternate WorkQueue");
            kernel::test::require(!alternate_queue.try_post(first) && !first.pending(),
                                  "WorkQueue 接受了已绑定其他队列的 Worklet");
            if (auto stopped = alternate_queue.shutdown(); !stopped)
                kernel::test::fail("alternate WorkQueue 关闭失败");
        }

        u64_t repost_dispatch_count = 0;
        u64_t repost_count          = 0;
        RepostWorklet repost(queue, repost_dispatch_count, repost_count, 1);
        kernel::test::require(queue.try_post(repost), "WorkQueue 拒绝 self-repost Worklet");
        scheduler::yield();
        kernel::test::require(repost_dispatch_count == 2 && repost_count == 1 &&
                                  !repost.pending() && queue.pending_count() == 0,
                              "WorkQueue 未准确完成一次 IDLE self-repost");

        TailDispatchState tail_dispatch_state{};
        auto *tail_dispatch_host = new (std::nothrow) TailDispatchHost(tail_dispatch_state);
        kernel::test::require(tail_dispatch_host != nullptr, "tail-dispatch host 分配失败");
        kernel::test::require(queue.try_post(tail_dispatch_host->worklet()),
                              "WorkQueue 拒绝 tail-dispatch 自销毁工作");

        u64_t post_destroy_counter = 0;
        CounterWorklet post_destroy_work(post_destroy_counter);
        kernel::test::require(queue.try_post(post_destroy_work),
                              "WorkQueue 拒绝自销毁宿主之后的工作");
        scheduler::yield();
        kernel::test::require(tail_dispatch_state.dispatched == 1 &&
                                  tail_dispatch_state.destroyed == 1 && post_destroy_counter == 1 &&
                                  !post_destroy_work.pending() && queue.pending_count() == 0,
                              "tail-dispatch 访问了已销毁宿主或阻塞队列");

        constexpr size_t FAIR_WORK_COUNT = kernel::async::WorkQueue::BATCH_BUDGET + 1;
        CounterWorklet fairness_work[FAIR_WORK_COUNT];
        u64_t fairness_counter = 0;
        for (auto &worklet : fairness_work) {
            worklet.setup(fairness_counter);
            kernel::test::require(queue.try_post(worklet), "WorkQueue 拒绝 batch fairness 工作");
        }

        FairnessState fairness_state{
            .queue   = &queue,
            .counter = &fairness_counter,
        };
        auto observer =
            task::Thread::create_kernel(task::kernel_proc(), fairness_observer, &fairness_state);
        if (!observer)
            kernel::test::fail("无法创建 WorkQueue fairness observer");
        if (auto attached = scheduler::attach(**observer); !attached)
            kernel::test::fail("无法发布 WorkQueue fairness observer");

        scheduler::yield();
        kernel::test::require(fairness_state.observer_ran && (*observer)->exited(),
                              "WorkQueue batch fairness observer 未运行");
        observer->reset();

        while (queue.pending_count() != 0) scheduler::yield();
        kernel::test::require(fairness_counter == FAIR_WORK_COUNT,
                              "WorkQueue batch 未准确分发全部 Worklet");

        u64_t shutdown_counter = 0;
        CounterWorklet shutdown_work(shutdown_counter);
        kernel::test::require(queue.try_post(shutdown_work), "WorkQueue 拒绝 shutdown drain 工作");
        if (auto stopped = queue.shutdown(); !stopped)
            kernel::test::fail("WorkQueue shutdown 失败");
        kernel::test::require(
            shutdown_counter == 1 && queue.stopped() && queue.pending_count() == 0,
            "WorkQueue shutdown 未 drain 并停止 worker");
        kernel::test::require(!queue.try_post(shutdown_work), "WorkQueue 在 shutdown 后仍接受工作");

        const auto statistics = queue.statistics();
        kernel::test::require(statistics.posted == FAIR_WORK_COUNT + 6 &&
                                  statistics.dispatched == statistics.posted &&
                                  statistics.maximum_depth >= FAIR_WORK_COUNT,
                              "WorkQueue 统计与 selftest 执行结果不匹配");
    }

    void run_smp_irq_queue(Context &) noexcept {
        const auto online = cpu::topology().snapshot().online;
        auto &queue       = kernel::async::bsp_work_queue();
        kernel::test::require(queue.accepting() && queue.pending_count() == 0,
                              "BSP WorkQueue 未处于可用空闲状态");
        const auto before_statistics = queue.statistics();

        IrqWorkQueueStressState stress{
            .queue    = &queue,
            .deadline = hal::Clock::instance().now() + (online.count() == 1 ? 45_s : 10_s),
        };
        // BSP timer completion 也必须覆盖；多 CPU 只增加跨 CPU producer 并发度。
        stress.timer_completion_enabled = true;
        stress.rounds                   = online.count() == 1 ? 8 : IRQ_WORK_ROUNDS;
        IrqWorkQueueProducerState producers[cpu::MAX_CPUS]{};
        cap::KObjectRef<task::Thread> workers[cpu::MAX_CPUS]{};
        size_t producer_count = 0;
        auto log_timeout      = [&](const char *phase) noexcept {
            kernel::log::info(
                "IRQ WorkQueue watchdog: phase={}, abort_reason={}, active={}, dispatched={}, "
                     "irq={}, producers_done={}",
                phase, stress.abort_reason.load(std::memory_order_acquire),
                stress.active_dispatches.load(std::memory_order_acquire),
                stress.dispatched.load(std::memory_order_acquire),
                stress.irq_dispatches.load(std::memory_order_acquire),
                stress.producers_done.load(std::memory_order_acquire));
            online.for_each([&](cpu::CpuId id) noexcept {
                kernel::log::info("  cpu={} posted={} completed={} phase={} exited={}", id.value,
                                       producers[id.value].posted.load(std::memory_order_acquire),
                                       producers[id.value].completed.load(std::memory_order_acquire),
                                       producers[id.value].phase.load(std::memory_order_acquire),
                                       !workers[id.value] || workers[id.value]->exited());
            });
        };
        online.for_each([&](cpu::CpuId id) noexcept {
            auto &producer  = producers[id.value];
            producer.stress = &stress;
            producer.target = id;
            for (auto &worklet : producer.worklets) worklet.bind(producer);
            if (stress.timer_completion_enabled) {
                producer.timer_worklet.bind(producer);
                producer.timer_worklet.mark_timer_done();
            }
            auto worker = task::Thread::create_kernel(task::kernel_proc(),
                                                      irq_queue_producer, &producer);
            if (!worker || !scheduler::attach(**worker, scheduler::Placement::Pinned(id)))
                kernel::test::fail("IRQ WorkQueue producer 发布失败");
            workers[id.value] = std::move(*worker);
            ++producer_count;
        });

        while (stress.producers_done.load(std::memory_order_acquire) != producer_count) {
            if (hal::Clock::instance().now() >= stress.deadline) {
                abort_irq_stress(stress, 10);
                online.for_each([&](cpu::CpuId id) noexcept {
                    kernel::log::info(
                        "IRQ WorkQueue timeout cpu={} phase={} posted={} completed={}", id.value,
                        producers[id.value].phase.load(std::memory_order_acquire),
                        producers[id.value].posted.load(std::memory_order_acquire),
                        producers[id.value].completed.load(std::memory_order_acquire));
                });
                kernel::test::fail("IRQ WorkQueue producer 超时");
            }
            scheduler::yield();
        }
        while (stress.active_dispatches.load(std::memory_order_acquire) != 0) {
            if (hal::Clock::instance().now() >= stress.deadline) {
                log_timeout("drain");
                kernel::test::fail("IRQ WorkQueue drain 超时");
            }
            scheduler::yield();
        }
        online.for_each([&](cpu::CpuId id) noexcept {
            while (!workers[id.value]->exited()) {
                if (hal::Clock::instance().now() >= stress.deadline) {
                    log_timeout("join");
                    kernel::test::fail("IRQ WorkQueue producer Thread 未退出");
                }
                scheduler::yield();
            }
            workers[id.value].reset();
        });

        // 即使压力阶段已经记录 abort，已发布的 Worklet 和 timer completion 仍须完成
        // 线性化后的 drain，避免失败路径把 BSP queue 或 timer node 带入下一个用例。
        const auto cleanup_deadline = hal::Clock::instance().now() + 2_s;
        while (true) {
            bool producers_drained = true;
            online.for_each([&](cpu::CpuId id) noexcept {
                const auto &producer  = producers[id.value];
                producers_drained    &= producer.posted.load(std::memory_order_acquire) ==
                                     producer.completed.load(std::memory_order_acquire);
                producers_drained &= kernel::timer::bsp_hrtimers().state(producer.timer_node) ==
                                     kernel::timer::HRTState::IDLE;
            });
            if (producers_drained && queue.pending_count() == 0 &&
                stress.active_dispatches.load(std::memory_order_acquire) == 0)
                break;
            if (hal::Clock::instance().now() >= cleanup_deadline) {
                log_timeout("abort_cleanup");
                kernel::test::fail("IRQ WorkQueue abort 路径未完成 queue/timer drain");
            }
            scheduler::yield();
        }

        if (stress.abort.load(std::memory_order_acquire)) {
            kernel::log::info(
                "IRQ WorkQueue stress abort: reason={}, active={}, dispatched={}, irq={}, "
                "producers_done={}",
                stress.abort_reason.load(std::memory_order_acquire),
                stress.active_dispatches.load(std::memory_order_acquire),
                stress.dispatched.load(std::memory_order_acquire),
                stress.irq_dispatches.load(std::memory_order_acquire),
                stress.producers_done.load(std::memory_order_acquire));
            online.for_each([&](cpu::CpuId id) noexcept {
                kernel::log::info("  producer cpu={} posted={} completed={} exited={}", id.value,
                                  producers[id.value].posted.load(std::memory_order_acquire),
                                  producers[id.value].completed.load(std::memory_order_acquire),
                                  !workers[id.value] || workers[id.value]->exited());
            });
        }
        kernel::test::require(!stress.abort.load(std::memory_order_acquire),
                              "IRQ WorkQueue stress 检测到重入、IRQ 状态或 deadline 错误");
        u64_t expected_posts       = 0;
        u64_t expected_completions = 0;
        online.for_each([&](cpu::CpuId id) noexcept {
            const auto &producer  = producers[id.value];
            expected_posts       += producer.posted.load(std::memory_order_acquire);
            expected_completions += producer.completed.load(std::memory_order_acquire);
            const auto expected_per_producer =
                stress.rounds * (IRQ_WORKLET_PER_ROUND + (stress.timer_completion_enabled ? 1 : 0));
            kernel::test::require(
                producer.posted.load(std::memory_order_acquire) == expected_per_producer &&
                    producer.completed.load(std::memory_order_acquire) ==
                        producer.posted.load(std::memory_order_acquire),
                "IRQ WorkQueue producer 计数不一致");
        });
        kernel::async::WorkQueueStats statistics{};
        while ((statistics = queue.statistics()).posted - before_statistics.posted <
                   expected_posts ||
               statistics.dispatched - before_statistics.dispatched < expected_completions)
        {
            if (hal::Clock::instance().now() >= stress.deadline) {
                log_timeout("statistics");
                kernel::test::fail("IRQ WorkQueue 统计更新超时");
            }
            scheduler::yield();
        }
        kernel::test::require(
            stress.dispatched.load(std::memory_order_acquire) == expected_completions &&
                expected_posts == expected_completions &&
                stress.irq_dispatches.load(std::memory_order_acquire) <= expected_completions &&
                statistics.posted - before_statistics.posted >= expected_posts &&
                statistics.dispatched - before_statistics.dispatched >= expected_completions &&
                statistics.maximum_depth >= 1,
            "IRQ WorkQueue posted/dispatched/depth 统计不一致");
    }
}  // namespace kernel::test::cases
