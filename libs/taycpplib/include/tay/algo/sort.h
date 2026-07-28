/**
 * @file sort.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 排序算法
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

namespace tay {
    namespace __algo {
        struct __sort {
            template <std::random_access_iterator I, std::sentinel_for<I> S,
                      class Comp = __less, class Proj = std::identity>
                requires requires(Comp& comp, Proj& proj,
                                  std::iter_reference_t<I> value) {
                    std::invoke(comp, std::invoke(proj, value),
                                std::invoke(proj, value));
                }
            constexpr I operator()(I first, S last, Comp comp = {},
                                   Proj proj = {}) const {
                I final = first;
                while (final != last) {
                    ++final;
                }

                if (first == final) {
                    return final;
                }

                // 目前先采取简单的插入排序算法.
                for (I current = first + 1; current != final; ++current) {
                    auto key = std::move(*current);
                    I hole   = current;

                    decltype(auto) projected_key = std::invoke(proj, key);
                    while (hole != first &&
                           std::invoke(comp, projected_key,
                                       std::invoke(proj, *(hole - 1))))
                    {
                        *hole = std::move(*(hole - 1));
                        --hole;
                    }
                    *hole = std::move(key);
                }
                return final;
            }

            template <random_access_range R, class Comp = __less,
                      class Proj = std::identity>
                requires requires(Comp& comp, Proj& proj,
                                  range_reference_t<R> value) {
                    std::invoke(comp, std::invoke(proj, value),
                                std::invoke(proj, value));
                }
            constexpr range_iterator_t<R> operator()(R&& r, Comp comp = {},
                                                     Proj proj = {}) const {
                return (*this)(begin(r), end(r), std::move(comp),
                               std::move(proj));
            }
        };
    }  // namespace __algo

    inline constexpr __algo::__sort sort{};
}  // namespace tay
