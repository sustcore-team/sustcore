/**
 * @file misc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief miscellaneous algorithms
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/algo/cmp.h>
#include <tay/algo/ranges.h>

#include <functional>
#include <utility>

namespace tay {
    namespace __algo {
        struct __unique {
            template <std::forward_iterator I, std::sentinel_for<I> S,
                      class Pred = __equal_to, class Proj = std::identity>
                requires requires(I iterator, Pred& pred, Proj& proj,
                                  std::iter_reference_t<I> left,
                                  std::iter_reference_t<I> right) {
                    std::invoke(pred, std::invoke(proj, left),
                                std::invoke(proj, right));
                    *iterator = std::move(*iterator);
                }
            constexpr I operator()(I first, S last, Pred pred = {},
                                   Proj proj = {}) const {
                if (first == last) {
                    return first;
                }

                I result = first;
                while (++first != last) {
                    if (!std::invoke(pred, std::invoke(proj, *result),
                                     std::invoke(proj, *first)))
                    {
                        ++result;
                        if (result != first) {
                            *result = std::move(*first);
                        }
                    }
                }
                ++result;
                return result;
            }

            template <forward_range R, class Pred = __equal_to,
                      class Proj = std::identity>
                requires requires(range_iterator_t<R> iterator, Pred& pred,
                                  Proj& proj, range_reference_t<R> left,
                                  range_reference_t<R> right) {
                    std::invoke(pred, std::invoke(proj, left),
                                std::invoke(proj, right));
                    *iterator = std::move(*iterator);
                }
            constexpr range_iterator_t<R> operator()(R&& range, Pred pred = {},
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(pred),
                               std::move(proj));
            }
        };

        struct __reverse {
            template <std::bidirectional_iterator I, std::sentinel_for<I> S>
                requires requires(I iterator) {
                    *iterator = std::move(*iterator);
                }
            constexpr I operator()(I first, S last) const {
                I final = first;
                while (final != last) {
                    ++final;
                }

                I tail = final;
                while (first != tail) {
                    --tail;
                    if (first == tail) {
                        break;
                    }

                    auto temporary = std::move(*first);
                    *first         = std::move(*tail);
                    *tail          = std::move(temporary);
                    ++first;
                }
                return final;
            }

            template <bidirectional_range R>
                requires requires(range_iterator_t<R> iterator) {
                    *iterator = std::move(*iterator);
                }
            constexpr range_iterator_t<R> operator()(R&& range) const {
                return (*this)(begin(range), end(range));
            }
        };
    }  // namespace __algo

    inline constexpr __algo::__unique unique{};
    inline constexpr __algo::__reverse reverse{};
}  // namespace tay
