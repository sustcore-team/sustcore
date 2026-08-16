/**
 * @file utility.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供 Tay C++ 库通用的移动、交换和类型工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace tay {
    template <typename Tag, typename T>
    struct composition : private T {
        constexpr composition() noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::is_default_constructible_v<T>
        = default;

        template <typename U>
            requires std::constructible_from<T, U &&>
        constexpr explicit composition(U &&value) noexcept(std::is_nothrow_constructible_v<T, U &&>)
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
        constexpr composition() noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::is_default_constructible_v<T>
        = default;

        template <typename U>
            requires std::constructible_from<T, U &&>
        constexpr explicit composition(U &&value) noexcept(std::is_nothrow_constructible_v<T, U &&>)
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

    namespace detail {
        struct projected_compare_compare_tag {};
        struct projected_compare_projection_tag {};
    }  // namespace detail

    /**
     * @brief 先投影左右操作数，再使用 Compare 比较投影结果。
     *
     * Compare 与 Projection 都作为对象保存，因此可以携带状态；空策略通过 composition
     * 使用空基类优化。operator() 保留左右操作数的值类别，并按底层调用传播 noexcept。
     */
    template <typename Compare, typename Projection>
    class projected_compare
        : private composition<detail::projected_compare_compare_tag, Compare>,
          private composition<detail::projected_compare_projection_tag, Projection> {
    private:
        using compare_base    = composition<detail::projected_compare_compare_tag, Compare>;
        using projection_base = composition<detail::projected_compare_projection_tag, Projection>;

        [[nodiscard]] constexpr Compare &compare() noexcept {
            return get<detail::projected_compare_compare_tag>(this);
        }

        [[nodiscard]] constexpr const Compare &compare() const noexcept {
            return get<detail::projected_compare_compare_tag>(this);
        }

        [[nodiscard]] constexpr Projection &projection() noexcept {
            return get<detail::projected_compare_projection_tag>(this);
        }

        [[nodiscard]] constexpr const Projection &projection() const noexcept {
            return get<detail::projected_compare_projection_tag>(this);
        }

    public:
        constexpr projected_compare(Compare compare, Projection projection) noexcept(
            std::is_nothrow_move_constructible_v<Compare> &&
            std::is_nothrow_move_constructible_v<Projection>)
            : compare_base(std::move(compare)), projection_base(std::move(projection)) {}

        template <typename Left, typename Right>
        [[nodiscard]] constexpr decltype(auto) operator()(Left &&left, Right &&right) noexcept(
            noexcept(std::invoke(compare(), std::invoke(projection(), std::forward<Left>(left)),
                                 std::invoke(projection(), std::forward<Right>(right))))) {
            return std::invoke(compare(), std::invoke(projection(), std::forward<Left>(left)),
                               std::invoke(projection(), std::forward<Right>(right)));
        }

        template <typename Left, typename Right>
        [[nodiscard]] constexpr decltype(auto) operator()(Left &&left, Right &&right) const
            noexcept(noexcept(std::invoke(compare(),
                                          std::invoke(projection(), std::forward<Left>(left)),
                                          std::invoke(projection(), std::forward<Right>(right))))) {
            return std::invoke(compare(), std::invoke(projection(), std::forward<Left>(left)),
                               std::invoke(projection(), std::forward<Right>(right)));
        }
    };

    template <typename... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;

        constexpr explicit overloaded(Ts... ts) noexcept(
            (std::is_nothrow_constructible_v<Ts, Ts &&> && ...))
            : Ts{std::move(ts)}... {}
    };

    template <typename... Ts>
    overloaded(Ts &&...) -> overloaded<std::decay_t<Ts>...>;

    template <typename T>
    struct dependent_false : std::false_type {};

    template <typename T>
    constexpr bool dependent_false_v = dependent_false<T>::value;
}  // namespace tay
