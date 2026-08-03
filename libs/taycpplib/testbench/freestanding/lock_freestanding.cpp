/**
 * @file lock_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 锁工具可在 freestanding 环境中编译和使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/lock.h>

#include <type_traits>
#include <utility>

namespace {
    struct context_guard {
        context_guard() noexcept             = default;
        context_guard(const context_guard &) = delete;
    };

    struct value {
        int count;
    };

    using context_lock =
        tay::context_lock_guard<tay::spinlock, tay::guard_stage<100, context_guard>>;
    using access_type = decltype(std::declval<tay::synchronized<value> &>().lock());

    static_assert(!std::is_copy_constructible_v<tay::spinlock>);
    static_assert(!std::is_move_constructible_v<tay::ticket_spinlock>);
    static_assert(!std::is_move_constructible_v<context_lock>);
    static_assert(!std::is_move_constructible_v<access_type>);
    static_assert(
        std::is_same_v<decltype(std::declval<const tay::synchronized<value> &>().lock().get()),
                       const value *>);
}  // namespace

void tay_lock_freestanding_contract() {
    tay::ticket_spinlock lock;
    if (lock.try_lock()) {
        lock.unlock();
    }

    tay::unique_lock deferred{lock, tay::defer_lock};
    if (deferred.try_lock()) {
        deferred.unlock();
    }

    tay::synchronized<value> state{value{1}};
    auto access = state.lock();
    ++access->count;
}
