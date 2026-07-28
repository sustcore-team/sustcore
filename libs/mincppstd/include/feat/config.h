/**
 * @file config.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief configuraions
 * @version alpha-1.0.0
 * @date 2026-03-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

namespace __config {
    // integral
    inline constexpr bool integral_traps = true;

    // float
    inline constexpr bool float_has_denorm_loss = false;
    inline constexpr bool float_traps = false;
    inline constexpr bool float_tinyness_before = false;

    // double
    inline constexpr bool double_has_denorm_loss = false;
    inline constexpr bool double_traps = false;
    inline constexpr bool double_tinyness_before = false;

    // long double
    inline constexpr bool long_double_has_denorm_loss = false;
    inline constexpr bool long_double_traps = false;
    inline constexpr bool long_double_tinyness_before = false;
}