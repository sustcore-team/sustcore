/**
 * @file cxa_guard.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief C++ 局部静态对象初始化 ABI 支持
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

// Itanium C++ ABI: 函数局部 static 的 guard 协议。
#include <cpu/local.h>
#include <log.h>
#include <synchronized.h>
#include <tay/bits.h>
#include <tay/spinlock.h>

namespace kcrt::detail::cxa_guard {
    constexpr u64_t INITIALIZED = 1;
    constexpr u64_t PENDING     = 2;
    constexpr size_t MAX_GUARDS = 256;

    struct GuardOwner final {
        u64_t *guard = nullptr;
        addr_t token = 0;
    };

    constinit GuardOwner owners[MAX_GUARDS]{};
    constinit tay::ticket_spinlock owner_lock;

    [[noreturn]] void invalid_context() noexcept {
        kernel::log::panic("禁止在中断或禁止抢占上下文初始化/等待局部静态对象");
    }

    [[nodiscard]] GuardOwner *find_owner(u64_t *guard) noexcept {
        for (auto &entry : owners)
            if (entry.guard == guard)
                return &entry;
        return nullptr;
    }

    void publish_owner(u64_t *guard, addr_t token) noexcept {
        if (auto *entry = find_owner(guard); entry != nullptr) {
            entry->token = token;
            return;
        }
        for (auto &entry : owners) {
            if (entry.guard != nullptr)
                continue;
            entry = GuardOwner{.guard = guard, .token = token};
            return;
        }
        kernel::log::panic("局部静态对象 guard 所有者表已耗尽");
    }

    void clear_owner(u64_t *guard) noexcept {
        if (auto *entry = find_owner(guard); entry != nullptr)
            *entry = {};
    }

    void wait_complete(u64_t *guard) noexcept {
        // 调用方已确认不是硬中断也未持有 preemption guard；因此持有者可经正常调度获得 CPU。
        while ((__atomic_load_n(guard, __ATOMIC_ACQUIRE) & PENDING) != 0)
            asm volatile("" ::: "memory");
    }
}  // namespace kcrt::detail::cxa_guard

extern "C" {
int __cxa_guard_acquire(u64_t *guard) {
    if (guard == nullptr)
        kernel::log::panic("局部静态对象 guard 为空");
    if (cpu::in_irq() || cpu::preempt_disabled())
        kcrt::detail::cxa_guard::invalid_context();

    const addr_t token = cpu::execution_token();
    while (true) {
        bool wait = false;
        {
            kernel::lock_guard<tay::ticket_spinlock> lock(kcrt::detail::cxa_guard::owner_lock);
            // acquire 保证观察到完成标志时，也能看到静态对象构造期间的全部写入。
            const u64_t state = __atomic_load_n(guard, __ATOMIC_ACQUIRE);
            if ((state & kcrt::detail::cxa_guard::INITIALIZED) != 0) {
                return 0;
            }
            if (state == 0) {
                u64_t expected_state = 0;
                if (__atomic_compare_exchange_n(guard, &expected_state,
                                                kcrt::detail::cxa_guard::PENDING, false,
                                                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
                {
                    kcrt::detail::cxa_guard::publish_owner(guard, token);
                    return 1;
                }
                continue;
            }
            if ((state & kcrt::detail::cxa_guard::PENDING) != 0) {
                const auto *owner = kcrt::detail::cxa_guard::find_owner(guard);
                if (owner == nullptr)
                    kernel::log::panic("局部静态对象 pending guard 缺少所有者");
                if (owner->token == token)
                    kernel::log::panic("局部静态对象初始化发生递归");
                // 释放 owner_lock 后等待；其它 Thread/CPU 的 PENDING 不是递归，不能 panic。
                wait = true;
            } else {
                kernel::log::panic("局部静态对象 guard 状态损坏");
            }
        }
        if (wait)
            kcrt::detail::cxa_guard::wait_complete(guard);
    }
}

void __cxa_guard_release(u64_t *guard) {
    // 构造完成后以 release 发布，随后读取到 INITIALIZED 的 CPU 可直接使用对象。
    __atomic_store_n(guard, u64_t{1}, __ATOMIC_RELEASE);
    kernel::lock_guard<tay::ticket_spinlock> lock(kcrt::detail::cxa_guard::owner_lock);
    kcrt::detail::cxa_guard::clear_owner(guard);
}

void __cxa_guard_abort(u64_t *guard) {
    // 清理 owner 与 PENDING 必须在同一临界区内完成；否则新一轮 acquire 可能在
    // 清零后抢先发布 owner，随后被本次 abort 误删。
    kernel::lock_guard<tay::ticket_spinlock> lock(kcrt::detail::cxa_guard::owner_lock);
    kcrt::detail::cxa_guard::clear_owner(guard);
    __atomic_store_n(guard, u64_t{0}, __ATOMIC_RELEASE);
}
}
