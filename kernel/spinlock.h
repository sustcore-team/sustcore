/**
 * @file spinlock.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 自旋锁
 * @version alpha-1.0.0
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/spinlock.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/spinlock.h>
#endif
#include <driver/int/base.h>
#include <features/attributes.h>
#include <task/scheduler.h>

class SpinLocker {
private:
    raw_spinlock_t _lock = 0;

public:
    SpinLocker() = default;

    void lock() {
        // TODO: 将当前线程标记为不可抢断
        // 更准确地说, 添加一个方法, 记录不可抢断的次数
        // 次数归零时取消不可抢断
        __raw_spin_lock(&_lock);
    }

    void unlock() {
        // TODO: 将当前线程取消不可抢断的标记
        __raw_spin_unlock(&_lock);
    }
};

template <typename GL>
concept GuardedLockLike = requires(SpinLocker &spinlock) { GL{spinlock}; };

class GuardedLock {
private:
    SpinLocker &_lock;
    bool locked                = false;
    bool _preempt_was_disabled = false;
    bool _irq_was_disabled     = false;

    __ATTR_ALWAYS_INLINE__
    bool protect_preempt() {
        if (!schd::Scheduler::initialized()) {
            return true;
        }

        auto &scheduler = schd::Scheduler::inst();
        auto *curtcb    = scheduler.current_tcb();
        if (curtcb == nullptr || scheduler.preempt_disabled()) {
            return true;
        }

        auto preempt_res = scheduler.preempt_disable();
        assert(preempt_res.has_value());
        return false;
    }

public:
    GuardedLock(SpinLocker &spinlock) : _lock(spinlock) {
        lock();
    }

    ~GuardedLock() {
        unlock();
    }

    void lock() {
        // make sure the action is atomatic
        InterruptGuard guard;
        guard.enter();

        if (!locked) {
            _preempt_was_disabled = protect_preempt();
            _lock.lock();
            locked = true;
        }
    }

    void unlock() {
        InterruptGuard guard;
        guard.enter();

        if (locked) {
            _lock.unlock();
            if (!_preempt_was_disabled && schd::Scheduler::initialized()) {
                auto preempt_res = schd::Scheduler::inst().preempt_enable();
                assert(preempt_res.has_value());
            }
            locked = false;
        }
    }
};

class IrqSaveGuardedLock {
private:
    SpinLocker &_lock;
    bool locked                = false;
    bool _preempt_was_disabled = false;
    bool _irq_was_disabled     = false;

    __ATTR_ALWAYS_INLINE__
    bool protect_preempt() {
        if (!schd::Scheduler::initialized()) {
            return true;
        }

        auto &scheduler = schd::Scheduler::inst();
        auto *curtcb    = scheduler.current_tcb();
        if (curtcb == nullptr || scheduler.preempt_disabled()) {
            return true;
        }

        auto preempt_res = scheduler.preempt_disable();
        assert(preempt_res.has_value());
        return false;
    }

    __ATTR_ALWAYS_INLINE__
    bool crit_enter() {
        if (!Interrupt::enabled()) {
            Interrupt::cli();
            return false;
        }
        return true;
    }

    __ATTR_ALWAYS_INLINE__
    void crit_leave()
    {
        Interrupt::sti();   
    }
public:
    IrqSaveGuardedLock(SpinLocker &spinlock) : _lock(spinlock) {
        lock();
    }

    ~IrqSaveGuardedLock() {
        unlock();
    }

    void lock() {
        _irq_was_disabled = crit_enter();

        if (!locked) {
            _preempt_was_disabled = protect_preempt();
            _lock.lock();
            locked = true;
        }
    }

    void unlock() {
        if (locked) {
            _lock.unlock();
            if (!_preempt_was_disabled && schd::Scheduler::initialized()) {
                auto preempt_res = schd::Scheduler::inst().preempt_enable();
                assert(preempt_res.has_value());
            }
            locked = false;
        }

        if (!_irq_was_disabled) {
            crit_leave();
        }
    }
};
