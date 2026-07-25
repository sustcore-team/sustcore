/**
 * @file range.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 区间相关操作
 * @version 1.0.0
 * @date 2026-04-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <compare>
#include <cstddef>
#include <sus/algobase.h>

namespace util {
    template <typename T>
    struct range {
        T begin;
        T end;

        constexpr range() = default;
        constexpr range(const range &other) = default;
        constexpr range(range &&other) = default;
        constexpr range &operator=(const range &other) = default;
        constexpr range &operator=(range &&other) = default;
        constexpr ~range() = default;

        constexpr range(T begin, T end) : begin(begin), end(end) {}

        [[nodiscard]]
        constexpr bool nullable() const {
            return begin >= end;
        }

        [[nodiscard]]
        constexpr size_t size() const {
            return end - begin;
        }
    };

    // 判断两个区间谁在前
    template <typename T>
    constexpr std::strong_ordering operator<=>(range<T> a, range<T> b) {
        return a.begin != b.begin ? a.begin <=> b.begin : a.end <=> b.end;
    }

    template <typename T>
    constexpr bool operator==(range<T> a, range<T> b)
    {
        return a.begin == b.begin && a.end == b.end;
    }

    // 计算两个区间的交集
    template <typename T>
    constexpr range<T> intersection(range<T> a, range<T> b)
    {
        T cbegin = max(a.begin, b.begin);
        T cend = min(a.end, b.end);
        if (cbegin >= cend) {
            return range<T>();
        }
        return range<T>(cbegin, cend);
    }

    template <typename T>
    constexpr bool is_intersecting(range<T> a, range<T> b)
    {
        return max(a.begin, b.begin) < min(a.end, b.end);
    }

    template <typename T>
    constexpr bool within(range<T> r, T v)
    {
        return r.begin <= v && v < r.end;
    }

    template <typename T>
    constexpr bool within(range<T> r, range<T> s)
    {
        return r.begin <= s.begin && s.end <= r.end;
    }
}  // namespace util::range