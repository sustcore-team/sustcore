/**
 * @file errors.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 错误类型
 * @version alpha-1.0.0
 * @date 2026-03-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <expected>
#include <functional>

namespace std {
    enum class error_type {
        NONE,
        OVERFLOW_ERROR,
        UNDERFLOW_ERROR,
        OUT_OF_RANGE
    };

    namespace __helper {
        template<typename T, typename E>
        struct __result {
            using type = std::expected<T, E>;
        };

        template<typename T, typename E>
        struct __result<T&, E> {
            using type = std::expected<std::reference_wrapper<T>, E>;
        };

        template<typename T, typename E>
        struct __result<const T&, E> {
            using type = std::expected<std::reference_wrapper<const T>, E>;
        };
    }

    template<typename T, typename E>
    using result = typename __helper::__result<T, E>::type;

    template <typename T>
    using result_nt = result<T, error_type>;
}