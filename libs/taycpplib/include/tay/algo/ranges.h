/**
 * @file ranges.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供容器范围访问与适配工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <utility>

namespace tay {
    namespace __algo {
        template <class T>
        constexpr T* __decay_copy(T* value) noexcept {
            return value;
        }

        struct __begin {
            template <class T, size_t N>
            [[nodiscard]]
            constexpr T* operator()(T (&arr)[N]) const noexcept {
                return arr;
            }

            template <class R>
                requires requires(R&& r) { std::forward<R>(r).begin(); }
            [[nodiscard]]
            constexpr auto operator()(R&& r) const noexcept(noexcept(std::forward<R>(r).begin())) {
                return std::forward<R>(r).begin();
            }
        };

        struct __end {
            template <class T, size_t N>
            [[nodiscard]]
            constexpr T* operator()(T (&arr)[N]) const noexcept {
                return arr + N;
            }

            template <class R>
                requires requires(R&& r) { std::forward<R>(r).end(); }
            [[nodiscard]]
            constexpr auto operator()(R&& r) const noexcept(noexcept(std::forward<R>(r).end())) {
                return std::forward<R>(r).end();
            }
        };

        inline constexpr __begin begin{};
        inline constexpr __end end{};

        struct __cbegin {
            template <class R>
            [[nodiscard]]
            constexpr auto operator()(const R& r) const noexcept(noexcept(begin(r))) {
                return begin(r);
            }
        };

        struct __cend {
            template <class R>
            [[nodiscard]]
            constexpr auto operator()(const R& r) const noexcept(noexcept(end(r))) {
                return end(r);
            }
        };

        struct __empty {
            template <class R>
                requires requires(R&& r) { std::forward<R>(r).empty(); }
            [[nodiscard]]
            constexpr bool operator()(R&& r) const
                noexcept(noexcept(static_cast<bool>(std::forward<R>(r).empty()))) {
                return static_cast<bool>(std::forward<R>(r).empty());
            }

            template <class T, size_t N>
            [[nodiscard]]
            constexpr bool operator()(T (&)[N]) const noexcept {
                return N == 0;
            }

            template <class R>
                requires(!requires(R&& r) { std::forward<R>(r).empty(); }) &&
                        requires(R&& r) { begin(r) == end(r); }
            [[nodiscard]]
            constexpr bool operator()(R&& r) const noexcept(noexcept(begin(r) == end(r))) {
                return begin(r) == end(r);
            }
        };

        struct __size {
            template <class R>
                requires requires(R&& r) { std::forward<R>(r).size(); }
            [[nodiscard]]
            constexpr auto operator()(R&& r) const noexcept(noexcept(std::forward<R>(r).size())) {
                return std::forward<R>(r).size();
            }

            template <class T, size_t N>
            [[nodiscard]]
            constexpr size_t operator()(T (&)[N]) const noexcept {
                return N;
            }

            template <class R>
                requires(!requires(R&& r) { std::forward<R>(r).size(); }) &&
                        requires(R&& r) { end(r) - begin(r); }
            [[nodiscard]]
            constexpr auto operator()(R&& r) const noexcept(noexcept(end(r) - begin(r))) {
                return end(r) - begin(r);
            }
        };

        struct __data {
            template <class R>
                requires requires(R&& r) { std::forward<R>(r).data(); }
            [[nodiscard]]
            constexpr auto operator()(R&& r) const noexcept(noexcept(std::forward<R>(r).data())) {
                return std::forward<R>(r).data();
            }

            template <class T, size_t N>
            [[nodiscard]]
            constexpr T* operator()(T (&arr)[N]) const noexcept {
                return arr;
            }

            template <class R>
                requires(!requires(R&& r) { std::forward<R>(r).data(); }) &&
                        requires(R&& r) { &*begin(r); }
            [[nodiscard]]
            constexpr auto operator()(R&& r) const noexcept(noexcept(&*begin(r))) {
                return &*begin(r);
            }
        };

        inline constexpr __cbegin cbegin{};
        inline constexpr __cend cend{};
        inline constexpr __empty empty{};
        inline constexpr __size size{};
        inline constexpr __data data{};

        template <class T>
        concept range = requires(T& t) {
            begin(t);
            end(t);
        };
    }  // namespace __algo

    template <class R>
    concept common_range = __algo::range<R>;

    template <common_range R>
    using range_iterator_t = decltype(__algo::begin(std::declval<R&>()));

    template <common_range R>
    using range_sentinel_t = decltype(__algo::end(std::declval<R&>()));

    template <common_range R>
    using range_reference_t = decltype(*__algo::begin(std::declval<R&>()));

    template <common_range R>
    using range_value_t = std::remove_cvref_t<range_reference_t<R>>;

    template <common_range R>
    using range_difference_t = std::iter_difference_t<range_iterator_t<R>>;

    template <class R>
    concept input_range = common_range<R> && std::input_iterator<range_iterator_t<R>>;

    template <class R>
    concept forward_range = common_range<R> && std::forward_iterator<range_iterator_t<R>>;

    template <class R>
    concept bidirectional_range =
        common_range<R> && std::bidirectional_iterator<range_iterator_t<R>>;

    template <class R, class T>
    concept output_range = common_range<R> && std::output_iterator<range_iterator_t<R>, T>;

    template <class R>
    concept random_access_range =
        common_range<R> && std::random_access_iterator<range_iterator_t<R>>;
}  // namespace tay
