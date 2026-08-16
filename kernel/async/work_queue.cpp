/**
 * @file work_queue.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief WorkQueue post、固定预算 drain、安全 park 与 shutdown 协议。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <async/work_queue.h>
#include <log.h>
#include <obj/process.h>
#include <scheduler/scheduler.h>
#include <tay/lock.h>

#include <utility>

namespace kernel::async {
    namespace {
        constinit WorkQueue bsp_queue(WorkQueue::PermanentTag{});
    }  // namespace

    WorkQueue &bsp_work_queue() noexcept {
        return bsp_queue;
    }

    WorkQueue::~WorkQueue() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        if (!pending_.empty() || worker_ref_ || worker_ != nullptr || accepting_ ||
            (started_ && !stopped_))
            kernel::log::panic("destroying an active WorkQueue");
    }

    tay::expected<void, WorkQueueError> WorkQueue::start(task::Process &process) noexcept {
        {
            hal::interrupt_guard interrupt_guard;
            tay::lock_guard guard(lock_);
            if (started_ || worker_ != nullptr || worker_ref_)
                return tay::Err(WorkQueueError::AlreadyStarted());
            if (!pending_.empty())
                return tay::Err(WorkQueueError::PendingWork());
        }

        auto created = task::Thread::create_kernel(process, worker_entry, this);
        if (!created)
            return tay::Err(WorkQueueError::WorkerCreationFailed(created.error().code()));

        hal::interrupt_guard interrupt_guard;
        {
            tay::lock_guard guard(lock_);
            if (started_ || worker_ != nullptr || worker_ref_ || !pending_.empty())
                kernel::log::panic("WorkQueue start 独占契约被并发破坏");
            worker_ref_         = std::move(*created);
            worker_             = worker_ref_.get();
            started_            = true;
            accepting_          = false;
            shutdown_requested_ = false;
            stopped_            = false;
        }

        auto attached = scheduler::instance().attach(*worker_);
        if (attached) {
            tay::lock_guard guard(lock_);
            if (!started_ || worker_ == nullptr || !worker_ref_ || accepting_ ||
                shutdown_requested_ || stopped_ || !pending_.empty())
                kernel::log::panic("WorkQueue worker attach 完成时启动状态不一致");
            accepting_ = true;
            return {};
        }

        cap::ObjectRef<task::Thread> failed_worker;
        {
            tay::lock_guard guard(lock_);
            failed_worker       = std::move(worker_ref_);
            worker_             = nullptr;
            started_            = false;
            accepting_          = false;
            shutdown_requested_ = false;
            stopped_            = false;
            if (!pending_.empty())
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
        task::Thread *worker_to_notify = nullptr;
        {
            hal::interrupt_guard interrupt_guard;
            {
                tay::lock_guard guard(lock_);
                if (!accepting_ || worker_ == nullptr ||
                    !(reserved ? worklet.try_claim_reserved_for_queue(*this)
                               : worklet.try_claim_for_queue(*this)))
                    return false;
                const bool was_empty = pending_.empty();
                pending_.push_back(&worklet);
                ++statistics_.posted;
                if (pending_.size() > statistics_.maximum_depth)
                    statistics_.maximum_depth = pending_.size();
                if (was_empty)
                    worker_to_notify = worker_;
            }
            // queue lock 必须先释放；notify 只取得 scheduler run queue lock。
            if (worker_to_notify != nullptr)
                scheduler::instance().notify_runnable_work(*worker_to_notify);
        }
        return true;
    }

    Worklet *WorkQueue::pop_front_locked() noexcept {
        if (pending_.empty())
            return nullptr;
        return pending_.pop_front();
    }

    size_t WorkQueue::run_batch(size_t budget) noexcept {
        size_t dispatched = 0;
        while (dispatched < budget) {
            Worklet *worklet = nullptr;
            {
                hal::interrupt_guard interrupt_guard;
                tay::lock_guard guard(lock_);
                worklet = pop_front_locked();
                if (worklet == nullptr)
                    break;
                worklet->release_to_dispatch();
            }

            // run() 可能销毁宿主；这是 WorkQueue 对 worklet 的最后一次访问。
            worklet->run();
            ++dispatched;
        }

        if (dispatched != 0) {
            hal::interrupt_guard interrupt_guard;
            tay::lock_guard guard(lock_);
            statistics_.dispatched += dispatched;
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
        auto &core = scheduler::instance();
        for (;;) {
            if (run_batch(BATCH_BUDGET) == BATCH_BUDGET) {
                scheduler::yield();
                continue;
            }

            hal::interrupt_guard interrupt_guard;
            lock_.lock();
            if (!pending_.empty()) {
                lock_.unlock();
                continue;
            }
            if (shutdown_requested_) {
                lock_.unlock();
                return;
            }

            // queue 判空与 RUNNING -> BLOCKING 在同一 queue lock 临界区中完成。
            auto token = core.prepare_block_current();
            lock_.unlock();
            if (!token)
                kernel::log::panic("WorkQueue worker failed to prepare park");
            if (auto committed = core.commit_block_current(std::move(*token)); !committed)
                kernel::log::panic("WorkQueue worker failed to commit park");
        }
    }

    tay::expected<void, WorkQueueError> WorkQueue::shutdown() noexcept {
        task::Thread *worker_to_notify = nullptr;
        {
            hal::interrupt_guard interrupt_guard;
            {
                tay::lock_guard guard(lock_);
                if (!started_ || worker_ == nullptr)
                    return tay::Err(WorkQueueError::NotStarted());
                if (permanent_)
                    return tay::Err(WorkQueueError::PermanentQueue());
                if (scheduler::instance().current() == worker_)
                    return tay::Err(WorkQueueError::CalledByWorker());
                accepting_          = false;
                shutdown_requested_ = true;
                if (!worker_->exited())
                    worker_to_notify = worker_;
            }
            if (worker_to_notify != nullptr)
                scheduler::instance().notify_runnable_work(*worker_to_notify);
        }

        for (;;) {
            {
                hal::interrupt_guard interrupt_guard;
                tay::lock_guard guard(lock_);
                if (worker_ != nullptr && worker_->exited())
                    break;
            }
            scheduler::yield();
        }

        cap::ObjectRef<task::Thread> stopped_worker;
        {
            hal::interrupt_guard interrupt_guard;
            tay::lock_guard guard(lock_);
            if (!pending_.empty() || worker_ == nullptr || !worker_->exited())
                kernel::log::panic("WorkQueue worker exited without draining the queue");
            stopped_worker = std::move(worker_ref_);
            worker_        = nullptr;
            stopped_       = true;
        }
        stopped_worker.reset();
        return {};
    }

    bool WorkQueue::accepting() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return accepting_;
    }

    bool WorkQueue::stopped() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return started_ && stopped_ && worker_ == nullptr && !worker_ref_;
    }

    size_t WorkQueue::pending_count() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return pending_.size();
    }

    WorkQueueStatistics WorkQueue::statistics() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return statistics_;
    }

    task::Thread *WorkQueue::worker() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(lock_);
        return worker_;
    }
}  // namespace kernel::async
