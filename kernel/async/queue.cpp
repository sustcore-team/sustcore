/**
 * @file queue.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief WorkQueue post、固定预算 drain、安全 park 与 shutdown 协议。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <async/queue.h>
#include <log.h>
#include <obj/process.h>
#include <scheduler/scheduler.h>

#include <utility>

namespace kernel::async {
    namespace {
        constinit WorkQueue bsp_queue(WorkQueue::PermanentTag{});
    }  // namespace

    WorkQueue &bsp_work_queue() noexcept {
        return bsp_queue;
    }

    WorkQueue::~WorkQueue() noexcept {
        auto state = state_.lock();
        if (!state->pending.empty() || state->worker_ref || state->worker != nullptr ||
            state->accepting || (state->started && !state->stopped))
            kernel::log::panic("destroying an active WorkQueue");
    }

    tay::expected<void, WorkQueueError> WorkQueue::start(task::Process &process,
                                                         scheduler::Placement placement) noexcept {
        {
            auto state = state_.lock();
            if (state->started || state->worker != nullptr || state->worker_ref)
                return tay::Err(WorkQueueError::AlreadyStarted());
            if (!state->pending.empty())
                return tay::Err(WorkQueueError::PendingWork());
        }

        auto created = task::Thread::create_kernel(process, worker_entry, this);
        if (!created)
            return tay::Err(WorkQueueError::WorkerCreateFailed(created.error().code()));

        task::Thread *worker = nullptr;
        {
            auto state = state_.lock();
            if (state->started || state->worker != nullptr || state->worker_ref ||
                !state->pending.empty())
                kernel::log::panic("WorkQueue start 独占契约被并发破坏");
            state->worker_ref = std::move(*created);
            state->worker     = state->worker_ref.get();
            state->started    = true;
            state->accepting  = false;
            state->stopping   = false;
            state->stopped    = false;
            worker            = state->worker;
        }

        auto attached = scheduler::attach(*worker, placement);
        if (attached) {
            auto state = state_.lock();
            if (!state->started || state->worker == nullptr || !state->worker_ref ||
                state->accepting || state->stopping || state->stopped || !state->pending.empty())
                kernel::log::panic("WorkQueue worker attach 完成时启动状态不一致");
            state->accepting = true;
            return {};
        }

        cap::KObjectRef<task::Thread> failed_worker;
        {
            auto state       = state_.lock();
            failed_worker    = std::move(state->worker_ref);
            state->worker    = nullptr;
            state->started   = false;
            state->accepting = false;
            state->stopping  = false;
            state->stopped   = false;
            if (!state->pending.empty())
                kernel::log::panic("WorkQueue attach 失败后遗留了 pending Worklet");
        }
        failed_worker.reset();
        return tay::Err(WorkQueueError::WorkerAttachFailed(attached.error().code()));
    }

    bool WorkQueue::try_post(Worklet &worklet) noexcept {
        return try_post_impl(worklet, false);
    }

    bool WorkQueue::try_post_reserved(Worklet &worklet) noexcept {
        return try_post_impl(worklet, true);
    }

    bool WorkQueue::try_post_impl(Worklet &worklet, bool reserved) noexcept {
        task::Thread *wake_worker = nullptr;
        {
            auto state = state_.lock();
            if (!state->accepting || state->worker == nullptr ||
                !(reserved ? worklet.try_claim_reserved(*this) : worklet.try_claim(*this)))
                return false;
            const bool was_empty = state->pending.empty();
            state->pending.push_back(&worklet);
            ++state->statistics.posted;
            if (state->pending.size() > state->statistics.maximum_depth)
                state->statistics.maximum_depth = state->pending.size();
            if (was_empty)
                wake_worker = state->worker;
        }

        // queue lock 必须先释放；notify 只取得 scheduler run queue lock。
        if (wake_worker != nullptr)
            scheduler::notify_work(*wake_worker);
        return true;
    }

    Worklet *WorkQueue::pop_front_locked(State &state) noexcept {
        if (state.pending.empty())
            return nullptr;
        return state.pending.pop_front();
    }

    size_t WorkQueue::run_batch(size_t budget) noexcept {
        size_t dispatched = 0;
        while (dispatched < budget) {
            Worklet *worklet = nullptr;
            {
                auto state = state_.lock();
                worklet    = pop_front_locked(*state);
                if (worklet == nullptr)
                    break;
                worklet->begin_dispatch();
            }

            // run() 可能销毁宿主；这是 WorkQueue 对 worklet 的最后一次访问。
            worklet->run();
            ++dispatched;
        }

        if (dispatched != 0) {
            auto state                    = state_.lock();
            state->statistics.dispatched += dispatched;
        }
        return dispatched;
    }

    void WorkQueue::worker_entry(void *opaque) noexcept {
        auto *queue = static_cast<WorkQueue *>(opaque);
        if (queue == nullptr)
            kernel::log::panic("WorkQueue worker has no queue");
        queue->worker_loop();
    }

    void WorkQueue::worker_loop() noexcept {
        auto &core = scheduler::local();
        while (true) {
            if (run_batch(BATCH_BUDGET) == BATCH_BUDGET) {
                scheduler::yield();
                continue;
            }

            // scheduler 的两阶段 park 要求从空队列检查到 commit 始终保持本地 IRQ-off；
            // state_ 的嵌套 guard 只负责在各次状态访问期间持有 queue lock。
            hal::irq_guard irq_guard;
            {
                auto state = state_.lock();
                if (!state->pending.empty())
                    continue;
                if (state->stopping)
                    return;
            }

            // 不持有 queue lock 取得目标 run queue lock。prepare 后重新检查 queue，
            // 覆盖 producer 在两阶段 park 窗口内发布 Worklet 的竞态。
            auto token = core.prepare_block();
            if (!token)
                kernel::log::panic("WorkQueue worker failed to prepare park");

            bool retry = false;
            bool stop  = false;
            {
                auto state = state_.lock();
                retry      = !state->pending.empty();
                stop       = state->stopping && !retry;
            }
            if (retry || stop) {
                core.cancel_block(std::move(*token));
                if (stop)
                    return;
                continue;
            }
            if (auto committed = core.commit_block(std::move(*token)); !committed)
                kernel::log::panic("WorkQueue worker failed to commit park");
        }
    }

    tay::expected<void, WorkQueueError> WorkQueue::shutdown() noexcept {
        task::Thread *wake_worker = nullptr;
        {
            auto state = state_.lock();
            if (!state->started || state->worker == nullptr)
                return tay::Err(WorkQueueError::NotStarted());
            if (permanent_)
                return tay::Err(WorkQueueError::PermanentQueue());
            if (scheduler::local().current() == state->worker)
                return tay::Err(WorkQueueError::CalledByWorker());
            state->accepting = false;
            state->stopping  = true;
            if (!state->worker->exited())
                wake_worker = state->worker;
        }
        if (wake_worker != nullptr)
            scheduler::notify_work(*wake_worker);

        while (true) {
            {
                auto state = state_.lock();
                if (state->worker != nullptr && state->worker->exited())
                    break;
            }
            scheduler::yield();
        }

        cap::KObjectRef<task::Thread> stopped_worker;
        {
            auto state = state_.lock();
            if (!state->pending.empty() || state->worker == nullptr || !state->worker->exited())
                kernel::log::panic("WorkQueue worker exited without draining the queue");
            stopped_worker = std::move(state->worker_ref);
            state->worker  = nullptr;
            state->stopped = true;
        }
        stopped_worker.reset();
        return {};
    }

    bool WorkQueue::accepting() noexcept {
        auto state = state_.lock();
        return state->accepting;
    }

    bool WorkQueue::stopped() noexcept {
        auto state = state_.lock();
        return state->started && state->stopped && state->worker == nullptr && !state->worker_ref;
    }

    size_t WorkQueue::pending_count() noexcept {
        auto state = state_.lock();
        return state->pending.size();
    }

    WorkQueueStats WorkQueue::statistics() noexcept {
        auto state = state_.lock();
        return state->statistics;
    }

    task::Thread *WorkQueue::worker() noexcept {
        auto state = state_.lock();
        return state->worker;
    }
}  // namespace kernel::async
