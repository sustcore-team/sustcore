/**
 * @file binary_search.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供支持投影的二分查找算法。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/algo/ranges.h>

#include <functional>
#include <iterator>
#include <utility>

namespace tay {
    namespace __algo {
        struct __lower_bound {
            template <std::forward_iterator I, std::sentinel_for<I> S, class T,
                      class Comp = std::ranges::less, class Proj = std::identity>
            [[nodiscard]] constexpr I operator()(I first, S last, const T& value, Comp comp = {},
                                                 Proj proj = {}) const {
                auto count = std::distance(first, last);
                while (count > 0) {
                    const auto step = count / 2;
                    I middle        = first;
                    for (std::iter_difference_t<I> offset = 0; offset < step; ++offset) {
                        ++middle;
                    }
                    if (std::invoke(comp, std::invoke(proj, *middle), value)) {
                        first  = ++middle;
                        count -= step + 1;
                    } else {
                        count = step;
                    }
                }
                return first;
            }
            template <forward_range R, class T, class Comp = std::ranges::less,
                      class Proj = std::identity>
            [[nodiscard]] constexpr range_iterator_t<R> operator()(R&& range, const T& value,
                                                                   Comp comp = {},
                                                                   Proj proj = {}) const {
                return (*this)(begin(range), end(range), value, std::move(comp), std::move(proj));
            }
        };

        struct __upper_bound {
            template <std::forward_iterator I, std::sentinel_for<I> S, class T,
                      class Comp = std::ranges::less, class Proj = std::identity>
            [[nodiscard]] constexpr I operator()(I first, S last, const T& value, Comp comp = {},
                                                 Proj proj = {}) const {
                auto count = std::distance(first, last);
                while (count > 0) {
                    const auto step = count / 2;
                    I middle        = first;
                    for (std::iter_difference_t<I> offset = 0; offset < step; ++offset) {
                        ++middle;
                    }
                    if (!std::invoke(comp, value, std::invoke(proj, *middle))) {
                        first  = ++middle;
                        count -= step + 1;
                    } else {
                        count = step;
                    }
                }
                return first;
            }
            template <forward_range R, class T, class Comp = std::ranges::less,
                      class Proj = std::identity>
            [[nodiscard]] constexpr range_iterator_t<R> operator()(R&& range, const T& value,
                                                                   Comp comp = {},
                                                                   Proj proj = {}) const {
                return (*this)(begin(range), end(range), value, std::move(comp), std::move(proj));
            }
        };

        struct __binary_search {
            template <std::forward_iterator I, std::sentinel_for<I> S, class T,
                      class Comp = std::ranges::less, class Proj = std::identity>
            [[nodiscard]] constexpr bool operator()(I first, S last, const T& value, Comp comp = {},
                                                    Proj proj = {}) const {
                I found = __lower_bound{}(first, last, value, comp, proj);
                return found != last && !std::invoke(comp, value, std::invoke(proj, *found));
            }
            template <forward_range R, class T, class Comp = std::ranges::less,
                      class Proj = std::identity>
            [[nodiscard]] constexpr bool operator()(R&& range, const T& value, Comp comp = {},
                                                    Proj proj = {}) const {
                return (*this)(begin(range), end(range), value, std::move(comp), std::move(proj));
            }
        };

        struct __equal_range {
            template <std::forward_iterator I, std::sentinel_for<I> S, class T,
                      class Comp = std::ranges::less, class Proj = std::identity>
            [[nodiscard]] constexpr std::pair<I, I> operator()(I first, S last, const T& value,
                                                               Comp comp = {},
                                                               Proj proj = {}) const {
                I lower = __lower_bound{}(first, last, value, comp, proj);
                I upper = __upper_bound{}(lower, last, value, comp, proj);
                return {lower, upper};
            }
            template <forward_range R, class T, class Comp = std::ranges::less,
                      class Proj = std::identity>
            [[nodiscard]] constexpr auto operator()(R&& range, const T& value, Comp comp = {},
                                                    Proj proj = {}) const {
                return (*this)(begin(range), end(range), value, std::move(comp), std::move(proj));
            }
        };
    }  // namespace __algo

    inline constexpr __algo::__lower_bound lower_bound{};
    inline constexpr __algo::__upper_bound upper_bound{};
    inline constexpr __algo::__binary_search binary_search{};
    inline constexpr __algo::__equal_range equal_range{};
}  // namespace tay
