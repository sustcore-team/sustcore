/**
 * @file timer_node.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 宿主嵌入、由精确定时器引擎临时借用的节点。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/bits.h>
#include <tay/list.h>
#include <tay/pairing_heap.h>
#include <tay/utility.h>

#include <functional>

namespace kernel::async {
    class Worklet;
}  // namespace kernel::async

namespace kernel::timer {
    class PrecisionTimerEngine;

    enum class PrecisionTimerState : u8_t {
        IDLE,
        QUEUED,
        ELAPSED,
        POSTED,
        RETIRED,
    };

    /**
     * @brief 非拥有式 timer node；宿主必须在 QUEUED/ELAPSED/POSTED 期间保持地址稳定。
     *
     * heap hook 与 due hook 属于两个不同容器。engine 只借用本节点和 completion，固定 BSP
     * WorkQueue 只借用已 RESERVED 的 completion；两者均不分配、移动或销毁宿主。
     */
    struct PrecisionTimerNode final {
        using heap_hook = tay::intrusive_pairing_heap_hook<PrecisionTimerNode>;
        using due_hook  = tay::intrusive_list_hook<PrecisionTimerNode *, PrecisionTimerNode *>;

        u64_t deadline_ = 0;
        heap_hook heap_hook_{};
        due_hook due_hook_{};
        async::Worklet *completion_   = nullptr;
        PrecisionTimerEngine *engine_ = nullptr;
        PrecisionTimerState state_    = PrecisionTimerState::IDLE;
    };

    using timer_heap_cmp = tay::projected_compare<std::ranges::less, u64_t PrecisionTimerNode::*>;
    using timer_heap_locator = tay::locate_member<PrecisionTimerNode, PrecisionTimerNode::heap_hook,
                                                  &PrecisionTimerNode::heap_hook_>;
    constexpr timer_heap_cmp TIMER_HEAP_CMP{std::ranges::less{}, &PrecisionTimerNode::deadline_};

    using timer_heap =
        tay::intrusive_pairing_heap<PrecisionTimerNode, timer_heap_locator, timer_heap_cmp>;
    using timer_due_list =
        tay::intrusive_list<PrecisionTimerNode,
                            tay::locate_member<PrecisionTimerNode, PrecisionTimerNode::due_hook,
                                               &PrecisionTimerNode::due_hook_>>;
}  // namespace kernel::timer
