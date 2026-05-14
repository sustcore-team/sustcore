/**
 * @file notif.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief notification对象
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <object/notif.h>
#include <device/int.h>
#include <task/wait.h>

namespace cap {
    NotificationPayload::NotificationPayload() : signalbits(0), wait_reasons{} {
        for (size_t i = 0; i < perm::notif::MAX_SIGNALS; ++i) {
            wait_reasons[i] = task::wait::alloc_reason();
        }
    }

    static Result<void> check_idx(size_t idx) {
        if (idx >= perm::notif::MAX_SIGNALS) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        void_return();
    }

    static Result<void> check_signal_perm(const Capability *cap, size_t idx) {
        auto permbits = perm::notif::perm(cap->perm(), idx);
        if ((permbits & perm::notif::SIGNAL) == 0) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        void_return();
    }

    static Result<void> check_query_perm(const Capability *cap, size_t idx) {
        auto permbits = perm::notif::perm(cap->perm(), idx);
        if ((permbits & perm::notif::QUERY) == 0) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        void_return();
    }

    Result<bool> NotificationObject::signal(size_t idx) {
        propagate(check_idx(idx));
        propagate(check_signal_perm(_cap, idx));

        InterruptGuard guard;
        guard.enter();
        _obj->signalbits |= (static_cast<b32>(1U) << idx);
        auto wake_res = task::wait::wake_all(_obj->wait_reasons[idx]);
        propagate(wake_res);
        return true;
    }

    Result<bool> NotificationObject::unsignal(size_t idx) {
        propagate(check_idx(idx));
        propagate(check_signal_perm(_cap, idx));

        InterruptGuard guard;
        guard.enter();
        _obj->signalbits &= ~(static_cast<b32>(1U) << idx);
        return false;
    }

    Result<bool> NotificationObject::set(size_t idx, bool state) {
        propagate(check_idx(idx));
        propagate(check_signal_perm(_cap, idx));

        InterruptGuard guard;
        guard.enter();
        if (state) {
            _obj->signalbits |= (static_cast<b32>(1U) << idx);
            auto wake_res = task::wait::wake_all(_obj->wait_reasons[idx]);
            propagate(wake_res);
        } else {
            _obj->signalbits &= ~(static_cast<b32>(1U) << idx);
        }
        return state;
    }

    Result<bool> NotificationObject::query(size_t idx) {
        propagate(check_idx(idx));
        propagate(check_query_perm(_cap, idx));

        InterruptGuard guard;
        guard.enter();
        return (_obj->signalbits & (static_cast<b32>(1U) << idx)) != 0;
    }

    Result<bool> NotificationObject::wait(size_t idx) {
        propagate(check_idx(idx));
        propagate(check_query_perm(_cap, idx));

        // The signal check and queue insertion must stay in one interrupt
        // critical section, otherwise a signal can be missed between them.
        InterruptGuard guard;
        guard.enter();

        // if the signal is already set, just return immediately without sleeping
        if ((_obj->signalbits & (static_cast<b32>(1U) << idx)) != 0) {
            return true;
        }

        auto wait_res = task::wait::wait_current(_obj->wait_reasons[idx]);
        propagate(wait_res);
        return true;
    }
}  // namespace cap
