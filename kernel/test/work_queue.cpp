/**
 * @file work_queue.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief WorkQueue 发布、调度、公平性与关闭 selftest。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <async/work_queue.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <test/cases.h>

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
    }  // namespace

    void run_work_queue(Context &) noexcept {
        kernel::async::WorkQueue queue;
        if (auto started = queue.start(task::kernel_process()); !started)
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
            if (auto started = alternate_queue.start(task::kernel_process()); !started)
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
            task::Thread::create_kernel(task::kernel_process(), fairness_observer, &fairness_state);
        if (!observer)
            kernel::test::fail("无法创建 WorkQueue fairness observer");
        if (auto attached = scheduler::instance().attach(**observer); !attached)
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
}  // namespace kernel::test::cases
