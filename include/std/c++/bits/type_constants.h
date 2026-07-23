/**
 * @file type_constants.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief type constants (true_type, false_type and etc.)
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

namespace std {
    // 基础模板 : integral_constant (整型常量包装器）
    template <typename T, T v>
    struct integral_constant {
        using value_type = T;
        using type       = integral_constant<T, v>;  // 自身类型

        static constexpr value_type value = v;

        // 转换函数 : 可以转换为值类型
        constexpr operator value_type() const noexcept {
            return value;
        }

        // 函数调用运算符 : 可以像函数一样使用
        constexpr value_type operator()() const noexcept {
            return value;
        }
    };

    // 布尔类型特化
    template <bool __v>
    using bool_constant = integral_constant<bool, __v>;
    // 实际上, 这并非标准库的一部分, 但是我们用的多, 所以就直接定义了
    template <size_t __v>
    using size_constant = integral_constant<size_t, __v>;
    using true_type     = bool_constant<true>;
    using false_type    = bool_constant<false>;
}  // namespace std
