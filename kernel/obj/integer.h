/**
 * @file integer.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability 基础设施自检使用的整数对象。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <obj/kernel_object.h>
#include <tay/counter.h>

namespace cap {
    /**
     * @brief selftest 使用的最小整数 Capability 对象。
     *
     * 对象只暴露不可变整数值；全局 live counter 用于验证最后 capability 引用和最后 pin
     * 分别在正确时机触发销毁。
     */
    class IntegerObject final : public TypedKernelObject<IntegerObject, ObjectType::INTEGER> {
    public:
        static constexpr ObjectType TYPE = ObjectType::INTEGER;

        /**
         * @brief 在内核堆上创建整数对象。
         * @param value 对象保存的整数值。
         * @return 成功时返回无 capability 引用的独占对象，分配失败时返回错误。
         */
        [[nodiscard]] static tay::expected<IntegerObject *, CapError> create(i64_t value) noexcept;

        explicit IntegerObject(i64_t value) noexcept;
        ~IntegerObject() noexcept;

        [[nodiscard]] i64_t value() const noexcept {
            return value_;
        }

        /** @brief 返回当前存活 IntegerObject 数量的原子快照。 */
        [[nodiscard]] static u32_t live_count() noexcept;

    private:
        i64_t value_ = 0;
        static tay::counter<u32_t> live_count_;
    };
}  // namespace cap
