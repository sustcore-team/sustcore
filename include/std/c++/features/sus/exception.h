/**
 * @file exception.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief exception
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/exception.h>
#include <features/sus/errors.h>

#include <type_traits>

/**
 * @brief This function will just print the exception
 * and then halt the system. It will never return.
 * @param s the exception to throw
 */
extern "C"
[[noreturn]]
void __sus_cxa_throw(const std::exception &e);

[[noreturn]]
inline void __sus_throw(const std::exception &e) {
    __sus_cxa_throw(e);
}

[[noreturn]]
inline void __sus_throw(std::exception &&e) {
    __sus_cxa_throw(e);
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define _THROW(exception) __sus_throw(exception)
#define _TRY
#define _CATCH(exception) if (0)
// NOLINTEND(cppcoreguidelines-macro-usage)