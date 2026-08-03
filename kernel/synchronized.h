/**
 * @file synchronized.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核中断并发环境共享对象同步抽象
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/interrupt.h>
#include <tay/lock.h>

namespace kernel {
    /**
     * @brief 在构造时关闭本地中断并获取 Lock，析构时按相反顺序恢复。
     * @tparam Lock 提供 lock()/unlock() 的自旋锁类型。
     */
    template <class Lock>
    using lock_guard = tay::context_lock_guard<Lock, tay::guard_stage<0, hal::interrupt_guard>>;

    /** @brief 使用 ticket spinlock 和本地中断保护封装共享对象。 */
    template <class T>
    using synchronized = tay::synchronized<T, tay::ticket_spinlock, lock_guard>;

    /** @brief 使用普通 spinlock 和本地中断保护封装共享对象。 */
    template <class T>
    using simple_synchronized = tay::synchronized<T, tay::spinlock, lock_guard>;

    /** @brief synchronized<T> 获取锁后返回的 ticket-spinlock 引用类型。 */
    template <class T>
    using locked_ref = tay::locked_ref<T, tay::ticket_spinlock, lock_guard>;

    /** @brief simple_synchronized<T> 获取锁后返回的引用类型。 */
    template <class T>
    using simple_locked_ref = tay::locked_ref<T, tay::spinlock, lock_guard>;
}  // namespace kernel
