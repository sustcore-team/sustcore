/**
 * @file worklet.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Worklet reservation、排队与 tail-dispatch 状态转换。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <async/worklet.h>
#include <log.h>

namespace kernel::async {
    Worklet::~Worklet() noexcept {
        if (state_.load(std::memory_order_acquire) != State::IDLE || queue_hook_.in_list)
            kernel::log::panic("destroying a Worklet still borrowed by timer or WorkQueue");
    }

    bool Worklet::pending() const noexcept {
        return state_.load(std::memory_order_acquire) != State::IDLE;
    }

    bool Worklet::try_reserve_timer(WorkQueue &queue) noexcept {
        return try_claim(State::IDLE, State::RESERVED, queue);
    }

    bool Worklet::try_claim(WorkQueue &queue) noexcept {
        return try_claim(State::IDLE, State::QUEUED, queue);
    }

    bool Worklet::try_claim_reserved(WorkQueue &queue) noexcept {
        return try_claim(State::RESERVED, State::QUEUED, queue);
    }

    bool Worklet::try_claim(State expected, State desired, WorkQueue &queue) noexcept {
        const auto original = expected;
        if (!state_.compare_exchange_strong(expected, desired, std::memory_order_acq_rel,
                                            std::memory_order_acquire))
            return false;

        WorkQueue *unbound = nullptr;
        if (queue_.compare_exchange_strong(unbound, &queue, std::memory_order_acq_rel,
                                           std::memory_order_acquire) ||
            unbound == &queue)
            return true;

        // 永久 queue affinity 防止 self-repost 被另一 worker 在本次 run() 返回前再次执行。
        state_.store(original, std::memory_order_release);
        return false;
    }

    void Worklet::begin_dispatch() noexcept {
        if (queue_hook_.in_list)
            kernel::log::panic("dispatching a Worklet with inconsistent queue membership");
        auto expected = State::QUEUED;
        if (!state_.compare_exchange_strong(expected, State::IDLE, std::memory_order_acq_rel,
                                            std::memory_order_acquire))
            kernel::log::panic("dispatching a Worklet outside the queued state");
    }
}  // namespace kernel::async
