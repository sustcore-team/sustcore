/**
 * @file kobject.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KObject 引用与 pin 生命周期实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/kobject.h>

namespace cap {
    namespace {
        constinit tay::counter<u64_t> object_ids{1};
    }

    KObject::KObject(ObjectType type, const KObjectOps *ops) noexcept {
        header_.type = type;
        header_.id   = KObjectId{.value = object_ids.next()};
        header_.ops  = ops;
    }

    bool KObject::try_pin() noexcept {
        if (state() != KObjectState::ALIVE)
            return false;

        header_.pins.acquire();
        if (state() == KObjectState::ALIVE)
            return true;

        unpin();
        return false;
    }

    void KObject::unpin() noexcept {
        if (header_.pins.release())
            try_retire();
    }

    void KObject::on_zero_references() noexcept {
        header_.state.store(KObjectState::RETIRING, std::memory_order_release);
        try_retire();
    }

    void KObject::try_retire() noexcept {
        if (strong_refs() != 0 || !header_.pins.empty()) {
            return;
        }

        auto expected = KObjectState::RETIRING;
        if (!header_.state.compare_exchange_strong(
                expected, KObjectState::DEAD, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return;
        }
        header_.ops->destroy(*this);
    }
}  // namespace cap
