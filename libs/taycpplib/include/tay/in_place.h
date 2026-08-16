/**
 * @file in_place.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay 原地构造使用的公共标签类型。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

namespace tay {
    struct in_place_t {
        explicit constexpr in_place_t() = default;
    };

    inline constexpr in_place_t in_place{};
}  // namespace tay
