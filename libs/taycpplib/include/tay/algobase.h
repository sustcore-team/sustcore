/**
 * @file algobase.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供 Tay C++ 库的基础算法实现集。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

namespace tay {
    /**
     * @brief 返回两个值中的较小者.
     *
     * @tparam T 值类型.
     * @param a 第一个值.
     * @param b 第二个值.
     * @return constexpr const T& 较小值的引用.
     */
    template <typename T>
    constexpr const T& min(const T& a, const T& b) {
        return (a < b) ? a : b;
    }

    /**
     * @brief 返回两个值中的较大者.
     *
     * @tparam T 值类型.
     * @param a 第一个值.
     * @param b 第二个值.
     * @return constexpr const T& 较大值的引用.
     */
    template <typename T>
    constexpr const T& max(const T& a, const T& b) {
        return (a > b) ? a : b;
    }

    /**
     * @brief 计算数值的绝对值.
     *
     * @tparam T 数值类型.
     * @param value 输入值.
     * @return constexpr T 绝对值结果.
     */
    template <typename T>
    constexpr T abs(const T& value) {
        return (value < 0) ? -value : value;
    }

    /**
     * @brief 将值钳制在给定闭区间内.
     *
     * @tparam T 值类型.
     * @param value 输入值.
     * @param low 下界.
     * @param high 上界.
     * @return constexpr T 钳制后的结果.
     */
    template <typename T>
    constexpr T clamp(const T& value, const T& low, const T& high) {
        return max(low, min(value, high));
    }

    /**
     * @brief 确认 value 是否形如 2^n 的形式
     *
     * 2^n 一定形如 100...000, 因此 2^n - 1 一定形如 011...111, 因此 2^n & (2^n - 1) == 0
     * 而当 v != 0 且 v ^ (v - 1) == 0 时, 若 v = 2^n + k (0 < k < 2^n), 则 v - 1 = 2^n + (k - 1)
     * 则 (v & (v - 1)) = (2^n + k) & (2^n + (k - 1)) > 2^n 因此 v ^ (v - 1) == 0 时, v 必然为 2^n
     *
     * @param value v
     * @return true   v == 2^n
     * @return false  v != 2^n
     */
    constexpr bool is_power_of_two(size_t value) noexcept {
        return value != 0 && (value & (value - 1)) == 0;
    }
}  // namespace tay