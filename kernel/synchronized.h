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

#include <cstddef>

namespace kernel {
    inline constexpr size_t PREEMPTION_GUARD_STAGE = 100;
    inline constexpr size_t INTERRUPT_GUARD_STAGE  = 200;

    /**
     * @brief 在构造时禁止本地抢占并获取 Lock，析构时按相反顺序恢复。
     * @tparam Lock 提供 lock()/unlock() 的自旋锁类型。
     */
    template <class Lock>
    using lock_guard =
        tay::context_lock_guard<Lock, tay::guard_stage<PREEMPTION_GUARD_STAGE, hal::preempt_guard>>;

    /** @brief 同时禁止抢占、关闭本地中断并获取 Lock。 */
    template <class Lock>
    using irq_lock_guard =
        tay::context_lock_guard<Lock, tay::guard_stage<PREEMPTION_GUARD_STAGE, hal::preempt_guard>,
                                tay::guard_stage<INTERRUPT_GUARD_STAGE, hal::irq_guard>>;

    /** @brief 使用 ticket spinlock 和抢占保护封装共享对象。 */
    template <class T>
    using synchronized = tay::synchronized<T, tay::ticket_spinlock, lock_guard>;

    /** @brief 使用普通 spinlock 和抢占保护封装共享对象。 */
    template <class T>
    using simple_synchronized = tay::synchronized<T, tay::spinlock, lock_guard>;

    /** @brief 在普通 Thread 与本地硬中断共享状态时使用的 irq-save 同步封装。 */
    template <class T>
    using irq_synchronized = tay::synchronized<T, tay::ticket_spinlock, irq_lock_guard>;

    template <class T>
    using irq_simple_synchronized = tay::synchronized<T, tay::spinlock, irq_lock_guard>;

    /** @brief synchronized<T> 获取锁后返回的 ticket-spinlock 引用类型。 */
    template <class T>
    using locked_ref = tay::locked_ref<T, tay::ticket_spinlock, lock_guard>;

    /** @brief simple_synchronized<T> 获取锁后返回的引用类型。 */
    template <class T>
    using simple_locked_ref = tay::locked_ref<T, tay::spinlock, lock_guard>;

    template <class T>
    using irq_locked_ref = tay::locked_ref<T, tay::ticket_spinlock, irq_lock_guard>;

    template <class T>
    using irq_simple_locked_ref = tay::locked_ref<T, tay::spinlock, irq_lock_guard>;
}  // namespace kernel
