/**
 * @file two_way.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 字符串 two-way 匹配算法声明
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

namespace tay::detail {
    enum class __search_direction { FORWARD, BACKWARD };

    /**
     * @brief Search for a byte pattern using the Two-Way algorithm.
     *
     * @tparam direction the compile-time search direction
     * @param text the text to search in
     * @param text_length the length of the text
     * @param pattern the pattern to search for
     * @param pattern_length the length of the pattern
     * @return The selected occurrence, or size_t(-1) if it is not found.
     */
    template <__search_direction direction>
    [[nodiscard]]
    size_t __str_two_way(const char* text, size_t text_length, const char* pattern,
                         size_t pattern_length) noexcept;
}  // namespace tay::detail
