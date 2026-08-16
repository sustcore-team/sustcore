/**
 * @file integer_object.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability selftest 使用的最小整数对象。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <obj/kernel_object.h>
#include <tay/counter.h>

namespace kernel::test::fixtures {
    /**
     * @brief 验证 Capability 引用与 pin 生命周期的测试夹具。
     *
     * 对象只暴露不可变整数值；全局 live counter 用于验证最后 capability 引用和最后 pin
     * 分别在正确时机触发销毁。
     */
    class IntegerObject final
        : public cap::TypedKernelObject<IntegerObject, cap::ObjectType::INTEGER> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::INTEGER;

        [[nodiscard]] static tay::expected<IntegerObject *, cap::CapError> create(
            i64_t value) noexcept;

        explicit IntegerObject(i64_t value) noexcept;
        ~IntegerObject() noexcept;

        [[nodiscard]] i64_t value() const noexcept {
            return value_;
        }

        [[nodiscard]] static u32_t live_count() noexcept;

    private:
        i64_t value_ = 0;
        static tay::counter<u32_t> live_count_;
    };
}  // namespace kernel::test::fixtures
