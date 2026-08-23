/**
 * @file mailbox.h
 * @brief 固定 CpuSlot 内嵌的 runtime IPI mailbox。
 */

#pragma once

#include <tay/bits.h>

#include <atomic>

namespace smp {
    /**
     * @brief 仅由 runtime IPI 协议访问的单 CPU mailbox。
     * @note pending 使用 release fetch_or/acquire exchange 配对；其它字段仅为无锁诊断计数。
     */
    struct alignas(64) IpiMailbox final {
        std::atomic<u32_t> pending{0};
        std::atomic<u64_t> posted{0};
        std::atomic<u64_t> coalesced{0};
        std::atomic<u64_t> notifications_needed{0};
        std::atomic<u64_t> dispatches{0};
        std::atomic<u64_t> reschedules{0};
        std::atomic<u64_t> tlb_shootdowns{0};
        std::atomic<u64_t> timer_deadlines{0};
        std::atomic<u64_t> stops{0};
    };

    static_assert(std::atomic<u32_t>::is_always_lock_free, "IPI pending mask must be lock-free");
    static_assert(std::atomic<u64_t>::is_always_lock_free,
                  "IPI diagnostic counters must be lock-free");
}  // namespace smp
