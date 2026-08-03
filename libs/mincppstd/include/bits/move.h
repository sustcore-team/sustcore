/**
 * @file move.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 mincppstd 的 C++ 标准库兼容层提供 move、forward 等值类别工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/qualifier.h>
#include <feature/attributes.h>

namespace std {
    template <typename T>
    [[nodiscard]]
    __ATTR_ALWAYS_INLINE__ constexpr T* __addressof(T& t) noexcept {
        return __builtin_addressof(t);
    }

    template <typename T>
    [[nodiscard]]
    __ATTR_ALWAYS_INLINE__ constexpr T* addressof(T& t) noexcept {
        return __builtin_addressof(t);
    }

    // 右值引用不应具有地址
    template <typename T>
    const T* addressof(const T&&) = delete;

    template <typename T>
    [[nodiscard]]
    __ATTR_ALWAYS_INLINE__ constexpr remove_reference_t<T>&& move(T&& t) {
        return static_cast<remove_reference_t<T>&&>(t);
    }

    template <typename T>
    [[nodiscard]]
    __ATTR_ALWAYS_INLINE__ constexpr T&& forward(remove_reference_t<T>& t) noexcept {
        return static_cast<T&&>(t);
    }

    template <typename T>
    [[nodiscard]]
    __ATTR_ALWAYS_INLINE__ constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
        return static_cast<T&&>(t);
    }

    namespace detailed {
        template <typename T, typename U>
        struct like_impl;

        template <typename T, typename U>
        struct like_impl<T&, U&> {
            using type = U&;
        };

        template <typename T, typename U>
        struct like_impl<const T&, U&> {
            using type = const U&;
        };

        template <typename T, typename U>
        struct like_impl<T&&, U&> {
            using type = U&&;
        };

        template <typename T, typename U>
        struct like_impl<const T&&, U&> {
            using type = const U&&;
        };

        template <typename T, typename U>
        using like_t = typename like_impl<T&&, U&>::type;

    }  // namespace detailed

    // forward with the cv-qualifiers and value category of another type
    // convert (cv U ref) -> (cv T ref)
    template <typename T, typename U>
    [[nodiscard]]
    __ATTR_ALWAYS_INLINE__ constexpr detailed::like_t<T, U> forward_like(U&& u) noexcept {
        return static_cast<detailed::like_t<T, U>>(u);
    }
}  // namespace std