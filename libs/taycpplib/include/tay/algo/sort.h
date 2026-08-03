/**
 * @file sort.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供排序算法。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/algo/cmp.h>
#include <tay/algo/ranges.h>
#include <tay/detail/introsort.h>

#include <functional>

namespace tay {
    namespace __algo {
        struct __sort {
            template <std::random_access_iterator I, std::sentinel_for<I> S,
                      class Comp = std::ranges::less, class Proj = std::identity>
                requires std::sortable<I, Comp, Proj>
            constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
                I final = first;
                while (final != last) {
                    ++final;
                }

                if (first == final) {
                    return final;
                }

                detail::introsort(first, final, std::move(comp), std::move(proj));
                return final;
            }

            template <random_access_range R, class Comp = std::ranges::less,
                      class Proj = std::identity>
                requires std::sortable<range_iterator_t<R>, Comp, Proj>
            constexpr range_iterator_t<R> operator()(R&& r, Comp comp = {}, Proj proj = {}) const {
                return (*this)(begin(r), end(r), std::move(comp), std::move(proj));
            }
        };
    }  // namespace __algo

    inline constexpr __algo::__sort sort{};
}  // namespace tay
