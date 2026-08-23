/**
 * @file queue.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 单 CPU 非拥有式 Worklet 队列与 kernel worker 生命周期。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <error/work_queue.h>
#include <async/worklet.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <synchronized.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <cstddef>

namespace task {
    class Process;
}

namespace kernel::timer {
    class HRTQueue;
}

namespace kernel::async {
    struct WorkQueueStats final {
        u64_t posted         = 0;
        u64_t dispatched     = 0;
        size_t maximum_depth = 0;
    };

    /**
     * @brief 单 worker WorkQueue；队列借用 Worklet，队列对象强持有其 worker Thread。
     *
     * 非永久 queue 析构前必须 shutdown()：先关闭 post 入口，再由 worker drain 已有 Worklet
     * 并退出，最后释放 Thread 强引用。宿主在 Worklet 被借用期间必须保持存活，但 run() 可以
     * 销毁宿主；worker 在虚调用前摘链并转回 IDLE，返回后不再访问 Worklet。queue affinity
     * 不拥有 Worklet，也不延长 WorkQueue 生命周期；PermanentTag 创建的 BSP queue 拒绝
     * shutdown 并具有固定生命周期。
     *
     * 首次成功 post/reservation 会记住 queue identity。非永久 queue 销毁后，曾经绑定
     * 它的 Worklet 不得再尝试 post；queue identity 只用于拒绝跨队列重入，不延长
     * WorkQueue 生命期。
     */
    class WorkQueue final {
    public:
        static constexpr size_t BATCH_BUDGET = 64;
        struct PermanentTag final {};

        constexpr WorkQueue() noexcept = default;
        explicit constexpr WorkQueue(PermanentTag) noexcept : permanent_(true) {}
        WorkQueue(const WorkQueue &)            = delete;
        WorkQueue &operator=(const WorkQueue &) = delete;
        WorkQueue(WorkQueue &&)                 = delete;
        WorkQueue &operator=(WorkQueue &&)      = delete;
        ~WorkQueue() noexcept;

        /**
         * @brief 创建并 attach worker；Scheduler attach 成功前不开放 post 入口。
         * @note 调用方在 start() 返回前必须独占本对象，不得并发调用 post 或 shutdown。
         */
        [[nodiscard]] tay::expected<void, WorkQueueError> start(
            task::Process &process,
            scheduler::Placement placement = scheduler::Placement::Pinned(cpu::CpuId{0})) noexcept;

        /**
         * @brief 发布未 pending 且已绑定本 queue 或尚未绑定的 Worklet。
         * @warning pending() 在 run() 期间也为 false；外部 producer 必须另行保证当前没有
         * run() 持有该 Worklet 的线性执行所有权。
         */
        [[nodiscard]] bool try_post(Worklet &worklet) noexcept;
        [[nodiscard]] tay::expected<void, WorkQueueError> shutdown() noexcept;

        [[nodiscard]] bool accepting() noexcept;
        [[nodiscard]] bool stopped() noexcept;
        [[nodiscard]] size_t pending_count() noexcept;
        [[nodiscard]] WorkQueueStats statistics() noexcept;
        [[nodiscard]] task::Thread *worker() noexcept;

    private:
        static void worker_entry(void *opaque) noexcept;
        void worker_loop() noexcept;
        [[nodiscard]] size_t run_batch(size_t budget = BATCH_BUDGET) noexcept;
        [[nodiscard]] bool try_post_impl(Worklet &worklet, bool reserved) noexcept;
        [[nodiscard]] bool try_post_reserved(Worklet &worklet) noexcept;

        struct State final {
            worklet_list pending{};
            cap::KObjectRef<task::Thread> worker_ref{};
            task::Thread *worker = nullptr;
            WorkQueueStats statistics{};
            bool started   = false;
            bool accepting = false;
            bool stopping  = false;
            bool stopped   = false;
        };

        [[nodiscard]] static Worklet *pop_front_locked(State &state) noexcept;

        kernel::irq_simple_synchronized<State> state_{};
        bool permanent_ = false;

        friend class kernel::timer::HRTQueue;
    };

    /**
     * @brief 返回 BSP 固定生命周期的通用 WorkQueue。
     * @note queue 本身不依赖动态初始化；worker 在 kernel process 与 scheduler 就绪后显式启动。
     */
    [[nodiscard]] WorkQueue &bsp_work_queue() noexcept;
}  // namespace kernel::async
