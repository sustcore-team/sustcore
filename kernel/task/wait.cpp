/**
 * @file wait.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief thread waiting reasons
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <env.h>
#include <task/scheduler.h>
#include <task/wait.h>

namespace task::wait {
    static util::Defer<WaitReasonManager> gWaitReasonMan;
    AutoDefer(gWaitReasonMan);

    WaitReasonManager &WaitReasonManager::inst() {
        return gWaitReasonMan.get();
    }

    WaitReasonId WaitReasonManager::alloc_reason() {
        return _next_reason++;
    }

    Result<WaitQueue *> WaitReasonManager::queue_for_wait(WaitReasonId id) {
        if (id == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto qres = _queues.get(id);
        if (qres.has_value()) {
            return qres.value().get();
        }

        // Queue allocation is intentionally lazy: owning a reason id does not
        // consume a wait queue until a thread actually blocks on that reason.
        auto *queue = new WaitQueue(id);
        if (queue == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        _queues.put(id, queue);
        return queue;
    }

    Result<WaitQueue *> WaitReasonManager::queue_if_exists(WaitReasonId id) {
        if (id == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto qres = _queues.get(id);
        if (!qres.has_value()) {
            // Waking an unused reason is a no-op, not an error.
            return static_cast<WaitQueue *>(nullptr);
        }
        return qres.value().get();
    }

    Result<void> WaitReasonManager::enqueue(WaitReasonId id, TCB *tcb) {
        if (tcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto qres = queue_for_wait(id);
        propagate(qres);

        tcb->wait_reason = id;
        tcb->basic_entity.state = ThreadState::WAITING;
        qres.value()->threads.push_back(*tcb);
        void_return();
    }

    Result<TCB *> WaitReasonManager::pop_one(WaitReasonId id) {
        auto qres = queue_if_exists(id);
        propagate(qres);

        WaitQueue *queue = qres.value();
        if (queue == nullptr || queue->threads.empty()) {
            return static_cast<TCB *>(nullptr);
        }

        TCB *tcb = &queue->threads.front();
        queue->threads.pop_front();
        tcb->wait_reason = 0;
        tcb->wait_head.clear();
        return tcb;
    }

    Result<size_t> WaitReasonManager::wake_one(WaitReasonId id) {
        auto pop_res = pop_one(id);
        propagate(pop_res);

        TCB *tcb = pop_res.value();
        if (tcb == nullptr) {
            return size_t(0);
        }

        auto *scheduler = env::inst().scheduler();
        if (scheduler == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return scheduler->wakeup_waiting(tcb) ? size_t(1) : size_t(0);
    }

    Result<size_t> WaitReasonManager::wake_all(WaitReasonId id) {
        size_t count = 0;
        while (true) {
            auto wake_res = wake_one(id);
            propagate(wake_res);
            if (wake_res.value() == 0) {
                return count;
            }
            count += wake_res.value();
        }
    }

    WaitReasonId alloc_reason() {
        return WaitReasonManager::inst().alloc_reason();
    }

    Result<void> wait_current(WaitReasonId id) {
        auto *scheduler = env::inst().scheduler();
        if (scheduler == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return scheduler->block_current(id);
    }

    Result<size_t> wake_one(WaitReasonId id) {
        return WaitReasonManager::inst().wake_one(id);
    }

    Result<size_t> wake_all(WaitReasonId id) {
        return WaitReasonManager::inst().wake_all(id);
    }
}  // namespace task::wait
