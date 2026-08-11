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
#include <tay/bits.h>

extern "C" {
int __cxa_guard_acquire(u64_t *guard) {
    constexpr u64_t INITIALIZED = 1;
    constexpr u64_t PENDING     = 2;
    while (true) {
        // acquire 保证观察到完成标志时，也能看到静态对象构造期间的全部写入。
        const u64_t state = __atomic_load_n(guard, __ATOMIC_ACQUIRE);
        if ((state & INITIALIZED) != 0) {
            return 0;
        }
        if (state == 0) {
            u64_t tmp = 0;
            // M0 只有 BSP；PENDING 被再次观察到时按递归初始化处理。
            if (__atomic_compare_exchange_n(guard, &tmp, PENDING, false, __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED))
            {
                return 1;
            }
            continue;
        }
        if ((state & PENDING) != 0) {
            // kernel::log::panic("局部静态对象初始化发生递归");
            while (true);
        }
    }
}

void __cxa_guard_release(u64_t *guard) {
    // 构造完成后以 release 发布，随后读取到 INITIALIZED 的 CPU 可直接使用对象。
    __atomic_store_n(guard, u64_t{1}, __ATOMIC_RELEASE);
}

void __cxa_guard_abort(u64_t *guard) {
    // 放弃初始化时清除 PENDING，允许后续调用重新认领。
    __atomic_store_n(guard, u64_t{0}, __ATOMIC_RELEASE);
}
}
