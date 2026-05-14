/**
 * @file mutex.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 互斥锁对象
 * @version alpha-1.0.0
 * @date 2026-05-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <object/mutex.h>
#include <device/int.h>

namespace cap {
    Result<bool> MutexObject::lock() {
        using namespace perm::mutex;
        if (!imply(USE)) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        InterruptGuard guard;
        guard.enter();
        if (_obj->locked) {
            return false;
        }
        _obj->locked = true;
        return true;
    }

    Result<bool> MutexObject::unlock() {
        using namespace perm::mutex;
        if (!imply(USE)) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        InterruptGuard guard;
        guard.enter();
        if (!_obj->locked) {
            return false;
        }
        _obj->locked = false;
        return true;
    }
}  // namespace cap