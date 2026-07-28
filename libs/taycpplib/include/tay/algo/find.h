/**
 * @file find.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief find algorithms
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/algo/ranges.h>

#include <functional>
#include <utility>

namespace tay {
    namespace __algo {
        struct __find {
            template <std::input_iterator I, std::sentinel_for<I> S, class T,
                      class Proj = std::identity>
                requires requires(Proj& proj, std::iter_reference_t<I> element,
                                  const T& value) {
                    std::invoke(proj, element) == value;
                }
            [[nodiscard]]
            constexpr I operator()(I first, S last, const T& value,
                                   Proj proj = {}) const {
                while (first != last) {
                    if (std::invoke(proj, *first) == value) {
                        return first;
                    }
                    ++first;
                }
                return first;
            }

            template <input_range R, class T, class Proj = std::identity>
                requires requires(Proj& proj, range_reference_t<R> element,
                                  const T& value) {
                    std::invoke(proj, element) == value;
                }
            [[nodiscard]]
            constexpr range_iterator_t<R> operator()(R&& range, const T& value,
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), value,
                               std::move(proj));
            }
        };

        struct __find_if {
            template <std::input_iterator I, std::sentinel_for<I> S, class Pred,
                      class Proj = std::identity>
                requires requires(Pred& pred, Proj& proj,
                                  std::iter_reference_t<I> element) {
                    std::invoke(pred, std::invoke(proj, element));
                }
            [[nodiscard]]
            constexpr I operator()(I first, S last, Pred pred,
                                   Proj proj = {}) const {
                while (first != last) {
                    if (std::invoke(pred, std::invoke(proj, *first))) {
                        return first;
                    }
                    ++first;
                }
                return first;
            }

            template <input_range R, class Pred, class Proj = std::identity>
                requires requires(Pred& pred, Proj& proj,
                                  range_reference_t<R> element) {
                    std::invoke(pred, std::invoke(proj, element));
                }
            [[nodiscard]]
            constexpr range_iterator_t<R> operator()(R&& range, Pred pred,
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(pred),
                               std::move(proj));
            }
        };

        struct __contains {
            template <std::input_iterator I, std::sentinel_for<I> S, class T,
                      class Proj = std::identity>
                requires requires(Proj& proj, std::iter_reference_t<I> element,
                                  const T& value) {
                    std::invoke(proj, element) == value;
                }
            [[nodiscard]]
            constexpr bool operator()(I first, S last, const T& value,
                                      Proj proj = {}) const {
                return __find{}(first, last, value, std::move(proj)) != last;
            }

            template <input_range R, class T, class Proj = std::identity>
                requires requires(Proj& proj, range_reference_t<R> element,
                                  const T& value) {
                    std::invoke(proj, element) == value;
                }
            [[nodiscard]]
            constexpr bool operator()(R&& range, const T& value,
                                      Proj proj = {}) const {
                return (*this)(begin(range), end(range), value,
                               std::move(proj));
            }
        };

        struct __remove_if {
            template <std::forward_iterator I, std::sentinel_for<I> S,
                      class Pred, class Proj = std::identity>
                requires requires(I iterator, Pred& pred, Proj& proj,
                                  std::iter_reference_t<I> element) {
                    std::invoke(pred, std::invoke(proj, element));
                    *iterator = std::move(*iterator);
                }
            constexpr I operator()(I first, S last, Pred pred,
                                   Proj proj = {}) const {
                while (first != last &&
                       !std::invoke(pred, std::invoke(proj, *first)))
                {
                    ++first;
                }
                if (first == last) {
                    return first;
                }

                I result = first;
                ++first;
                while (first != last) {
                    if (!std::invoke(pred, std::invoke(proj, *first))) {
                        *result = std::move(*first);
                        ++result;
                    }
                    ++first;
                }
                return result;
            }

            template <forward_range R, class Pred, class Proj = std::identity>
                requires requires(Pred& pred, Proj& proj,
                                  range_reference_t<R> element) {
                    std::invoke(pred, std::invoke(proj, element));
                }
            constexpr range_iterator_t<R> operator()(R&& range, Pred pred,
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(pred),
                               std::move(proj));
            }
        };

        struct __remove {
            template <std::forward_iterator I, std::sentinel_for<I> S, class T,
                      class Proj = std::identity>
                requires requires(Proj& proj, std::iter_reference_t<I> element,
                                  const T& value) {
                    std::invoke(proj, element) == value;
                }
            constexpr I operator()(I first, S last, const T& value,
                                   Proj proj = {}) const {
                return __remove_if{}(
                    first, last,
                    [&value](const auto& element) { return element == value; },
                    std::move(proj));
            }

            template <forward_range R, class T, class Proj = std::identity>
                requires requires(Proj& proj, range_reference_t<R> element,
                                  const T& value) {
                    std::invoke(proj, element) == value;
                }
            constexpr range_iterator_t<R> operator()(R&& range, const T& value,
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), value,
                               std::move(proj));
            }
        };
    }  // namespace __algo

    inline constexpr __algo::__find find{};
    inline constexpr __algo::__find_if find_if{};
    inline constexpr __algo::__contains contains{};
    inline constexpr __algo::__remove remove{};
    inline constexpr __algo::__remove_if remove_if{};
}  // namespace tay
