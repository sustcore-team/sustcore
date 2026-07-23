/**
 * @file stdlib_assert.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief stdlib assert
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

namespace std {
    [[noreturn]]
    void __stdlib_assert_fail(const char *__file, int __line,
                              const char *__function, const char *__condition);
}  // namespace std

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define __stdlib_assert(_Condition)                                            \
    do {                                                                       \
        if (!(_Condition)) {                                                   \
            std::__stdlib_assert_fail(__FILE__, __LINE__, __PRETTY_FUNCTION__, \
                                      #_Condition);                            \
        }                                                                      \
    } while (false)
// NOLINTEND(cppcoreguidelines-macro-usage)