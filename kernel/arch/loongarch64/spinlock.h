/**
 * @file spinlock.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 自旋锁
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <features/attributes.h>

#include <cstdint>

using raw_spinlock_t = volatile uint32_t;

__ATTR_ALWAYS_INLINE__
static void __raw_spin_lock(raw_spinlock_t *lock) {
    uint32_t expected;
    do {
        expected = __atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE);
    } while (expected != 0);
}

__ATTR_ALWAYS_INLINE__
static void __raw_spin_unlock(raw_spinlock_t *lock) {
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}
