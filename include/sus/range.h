/**
 * @file range.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 区间相关操作
 * @version alpha-1.0.0
 * @date 2026-04-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <compare>
#include <algorithm>
#include <cstddef>

namespace util::range {
    template <typename T>
    struct Range {
        T begin;
        T end;

        constexpr Range() = default;
        constexpr Range(const Range &other) = default;
        constexpr Range(Range &&other) = default;
        constexpr Range &operator=(const Range &other) = default;
        constexpr Range &operator=(Range &&other) = default;
        constexpr ~Range() = default;

        constexpr Range(T begin, T end) : begin(begin), end(end) {}

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
    constexpr std::strong_ordering operator<=>(Range<T> a, Range<T> b) {
        return a.begin != b.begin ? a.begin <=> b.begin : a.end <=> b.end;
    }

    template <typename T>
    constexpr bool operator==(Range<T> a, Range<T> b)
    {
        return a.begin == b.begin && a.end == b.end;
    }

    // 计算两个区间的交集
    template <typename T>
    constexpr Range<T> intersection(Range<T> a, Range<T> b)
    {
        T cbegin = std::max(a.begin, b.begin);
        T cend = std::min(a.end, b.end);
        if (cbegin >= cend) {
            return Range<T>();
        }
        return Range<T>(cbegin, cend);
    }

    template <typename T>
    constexpr bool is_intersecting(Range<T> a, Range<T> b)
    {
        return std::max(a.begin, b.begin) < std::min(a.end, b.end);
    }

    template <typename T>
    constexpr bool within(Range<T> range, T value)
    {
        return range.begin <= value && value < range.end;
    }

    template <typename T>
    constexpr bool within(Range<T> range, Range<T> other)
    {
        return range.begin <= other.begin && other.end <= range.end;
    }
}  // namespace util::range