/**
 * @file unique_lock_panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::unique_lock 非法使用时的 panic 行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/lock.h>

int main() {
#if defined(TAY_TEST_UNIQUE_LOCK_NO_MUTEX)
    tay::unique_lock<tay::spinlock> lock;
    lock.lock();
#elif defined(TAY_TEST_UNIQUE_LOCK_ALREADY_OWNS)
    tay::spinlock mutex;
    tay::unique_lock lock{mutex};
    lock.lock();
#elif defined(TAY_TEST_UNIQUE_LOCK_NOT_OWNED)
    tay::spinlock mutex;
    tay::unique_lock lock{mutex, tay::defer_lock};
    lock.unlock();
#else
#error "unique_lock panic scenario is not selected"
#endif
}
