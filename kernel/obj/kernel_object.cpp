/**
 * @file kernel_object.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KernelObject 引用与 pin 生命周期实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/kernel_object.h>

namespace cap {
    namespace {
        constinit tay::counter<u64_t> object_ids{1};
    }

    KernelObject::KernelObject(ObjectType type, const ObjectOps *ops) noexcept {
        header_.type = type;
        header_.id   = ObjectId{object_ids.next()};
        header_.ops  = ops;
    }

    bool KernelObject::try_pin() noexcept {
        if (state() != ObjectState::ALIVE)
            return false;

        header_.kernel_pins.acquire();
        if (state() == ObjectState::ALIVE)
            return true;

        unpin();
        return false;
    }

    void KernelObject::unpin() noexcept {
        if (header_.kernel_pins.release())
            try_retire();
    }

    void KernelObject::on_zero_references() noexcept {
        header_.state.store(ObjectState::RETIRING, std::memory_order_release);
        try_retire();
    }

    void KernelObject::try_retire() noexcept {
        if (object_refs() != 0 || !header_.kernel_pins.empty()) {
            return;
        }

        auto expected = ObjectState::RETIRING;
        if (!header_.state.compare_exchange_strong(
                expected, ObjectState::DEAD, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return;
        }
        header_.ops->destroy(*this);
    }
}  // namespace cap
