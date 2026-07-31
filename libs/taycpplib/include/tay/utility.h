/**
 * @file utility.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief utilities for taycpp
 * @version 0.1.0-dev.1
 * @date 2026-07-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace tay {
    template <typename Tag, typename T>
    struct composition : private T {
        constexpr composition() noexcept(
            std::is_nothrow_default_constructible_v<T>)
            requires std::is_default_constructible_v<T>
        = default;

        template <typename U>
            requires std::constructible_from<T, U &&>
        constexpr explicit composition(U &&value) noexcept(
            std::is_nothrow_constructible_v<T, U &&>)
            : T(std::forward<U>(value)) {}

        static constexpr T &get(composition<Tag, T> *p) noexcept {
            return *static_cast<T *>(p);
        }

        static constexpr const T &get(const composition<Tag, T> *p) noexcept {
            return *static_cast<const T *>(p);
        }
    };

    template <typename Tag, typename T>
        requires(!std::is_class_v<T>)
    struct composition<Tag, T> {
        constexpr composition() noexcept(
            std::is_nothrow_default_constructible_v<T>)
            requires std::is_default_constructible_v<T>
        = default;

        template <typename U>
            requires std::constructible_from<T, U &&>
        constexpr explicit composition(U &&value) noexcept(
            std::is_nothrow_constructible_v<T, U &&>)
            : value_(std::forward<U>(value)) {}

        static constexpr T &get(composition<Tag, T> *p) noexcept {
            return p->value_;
        }

        static constexpr const T &get(const composition<Tag, T> *p) noexcept {
            return p->value_;
        }

    private:
        T value_;
    };

    template <typename Tag, typename T>
    constexpr T &get(composition<Tag, T> *p) noexcept {
        return composition<Tag, T>::get(p);
    }

    template <typename Tag, typename T>
    constexpr const T &get(const composition<Tag, T> *p) noexcept {
        return composition<Tag, T>::get(p);
    }

    template <typename... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;

        constexpr explicit overloaded(Ts... ts) noexcept(
            (std::is_nothrow_constructible_v<Ts, Ts &&> && ...))
            : Ts{std::move(ts)}... {}
    };

    template <typename... Ts>
    overloaded(Ts &&...) -> overloaded<std::decay_t<Ts>...>;
}  // namespace tay
