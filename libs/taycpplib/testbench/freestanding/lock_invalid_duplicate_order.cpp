/**
 * @file lock_invalid_duplicate_order.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 作为 freestanding 编译失败用例，验证锁顺序重复定义的诊断。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/lock.h>

namespace {
    struct first_guard {};
    struct second_guard {};

    using invalid_guard = tay::context_lock_guard<tay::spinlock, tay::guard_stage<100, first_guard>,
                                                  tay::guard_stage<100, second_guard>>;
}  // namespace

void duplicate_context_guard_order_is_invalid(tay::spinlock& lock) {
    invalid_guard held{lock};
}
