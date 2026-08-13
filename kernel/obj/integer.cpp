/**
 * @file integer.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief IntegerObject 生命周期实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/integer.h>

#include <new>

namespace cap {
    constinit tay::counter<u32_t> IntegerObject::live_count_{0};

    tay::expected<IntegerObject *, CapError> IntegerObject::create(i64_t value) noexcept {
        auto *object = new (std::nothrow) IntegerObject(value);
        if (object == nullptr)
            return tay::Err(CapError::OUT_OF_MEMORY);
        return object;
    }

    IntegerObject::IntegerObject(i64_t value) noexcept : value_(value) {
        static_cast<void>(live_count_.increment());
    }

    IntegerObject::~IntegerObject() noexcept {
        static_cast<void>(live_count_.decrement());
    }

    u32_t IntegerObject::live_count() noexcept {
        return live_count_.value(std::memory_order_acquire);
    }
}  // namespace cap
