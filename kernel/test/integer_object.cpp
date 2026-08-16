/**
 * @file integer_object.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability selftest 整数对象的生命周期实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <test/integer_object.h>

#include <new>

namespace kernel::test::fixtures {
    constinit tay::counter<u32_t> IntegerObject::live_count_{0};

    tay::expected<IntegerObject *, cap::CapError> IntegerObject::create(i64_t value) noexcept {
        auto *object = new (std::nothrow) IntegerObject(value);
        if (object == nullptr)
            return tay::Err(cap::CapError::OutOfMemory());
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
}  // namespace kernel::test::fixtures
