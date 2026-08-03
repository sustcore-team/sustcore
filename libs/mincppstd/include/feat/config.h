/**
 * @file config.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 配置 mincppstd C++ 标准库头兼容层的目标环境和编译器能力。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
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
    inline constexpr bool float_traps           = false;
    inline constexpr bool float_tinyness_before = false;

    // double
    inline constexpr bool double_has_denorm_loss = false;
    inline constexpr bool double_traps           = false;
    inline constexpr bool double_tinyness_before = false;

    // long double
    inline constexpr bool long_double_has_denorm_loss = false;
    inline constexpr bool long_double_traps           = false;
    inline constexpr bool long_double_tinyness_before = false;
}  // namespace __config