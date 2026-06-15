/**
 * @file mutex.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 互斥锁对象
 * @version alpha-1.0.0
 * @date 2026-05-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <cap/capability.h>
#include <object/perm.h>

namespace cap {
    struct MutexPayload : public _PayloadHelper<PayloadType::MUTEX> {
        bool locked = false;
    };

    class MutexObject : public CapObj<MutexPayload> {
    public:
        explicit MutexObject(util::nonnull<Capability *> cap)
            : CapObj<MutexPayload>(cap) {}

        Result<bool> lock();
        Result<bool> unlock();
    };
}  // namespace cap
