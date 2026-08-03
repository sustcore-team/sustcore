/**
 * @file heap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供支持投影的二叉堆算法。
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
        template <std::random_access_iterator I, class Comp, class Proj>
        constexpr void __sift_down(I first, std::iter_difference_t<I> root,
                                   std::iter_difference_t<I> count, Comp& comp, Proj& proj) {
            while (root * 2 + 1 < count) {
                auto child = root * 2 + 1;
                if (child + 1 < count && std::invoke(comp, std::invoke(proj, first[child]),
                                                     std::invoke(proj, first[child + 1])))
                {
                    ++child;
                }
                if (!std::invoke(comp, std::invoke(proj, first[root]),
                                 std::invoke(proj, first[child])))
                {
                    return;
                }
                std::ranges::iter_swap(first + root, first + child);
                root = child;
            }
        }

        struct __make_heap {
            template <std::random_access_iterator I, std::sentinel_for<I> S,
                      class Comp = std::ranges::less, class Proj = std::identity>
            constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
                I final = first;
                while (final != last) ++final;
                const auto count = final - first;
                if (count > 1) {
                    for (auto root = (count - 2) / 2 + 1; root > 0;) {
                        --root;
                        __sift_down(first, root, count, comp, proj);
                    }
                }
                return final;
            }
            template <random_access_range R, class Comp = std::ranges::less,
                      class Proj = std::identity>
            constexpr range_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(comp), std::move(proj));
            }
        };

        struct __push_heap {
            template <std::random_access_iterator I, std::sentinel_for<I> S,
                      class Comp = std::ranges::less, class Proj = std::identity>
            constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
                I final = first;
                while (final != last) ++final;
                auto child = final - first - 1;
                while (child > 0) {
                    const auto parent = (child - 1) / 2;
                    if (!std::invoke(comp, std::invoke(proj, first[parent]),
                                     std::invoke(proj, first[child])))
                        break;
                    std::ranges::iter_swap(first + parent, first + child);
                    child = parent;
                }
                return final;
            }
            template <random_access_range R, class Comp = std::ranges::less,
                      class Proj = std::identity>
            constexpr range_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(comp), std::move(proj));
            }
        };

        struct __pop_heap {
            template <std::random_access_iterator I, std::sentinel_for<I> S,
                      class Comp = std::ranges::less, class Proj = std::identity>
            constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
                I final = first;
                while (final != last) ++final;
                const auto count = final - first;
                if (count > 1) {
                    std::ranges::iter_swap(first, final - 1);
                    __sift_down(first, 0, count - 1, comp, proj);
                }
                return final;
            }
            template <random_access_range R, class Comp = std::ranges::less,
                      class Proj = std::identity>
            constexpr range_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                     Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(comp), std::move(proj));
            }
        };

        struct __is_heap {
            template <std::random_access_iterator I, std::sentinel_for<I> S,
                      class Comp = std::ranges::less, class Proj = std::identity>
            [[nodiscard]] constexpr bool operator()(I first, S last, Comp comp = {},
                                                    Proj proj = {}) const {
                I final = first;
                while (final != last) ++final;
                const auto count = final - first;
                for (std::iter_difference_t<I> child = 1; child < count; ++child) {
                    const auto parent = (child - 1) / 2;
                    if (std::invoke(comp, std::invoke(proj, first[parent]),
                                    std::invoke(proj, first[child])))
                        return false;
                }
                return true;
            }
            template <random_access_range R, class Comp = std::ranges::less,
                      class Proj = std::identity>
            [[nodiscard]] constexpr bool operator()(R&& range, Comp comp = {},
                                                    Proj proj = {}) const {
                return (*this)(begin(range), end(range), std::move(comp), std::move(proj));
            }
        };
    }  // namespace __algo

    inline constexpr __algo::__make_heap make_heap{};
    inline constexpr __algo::__push_heap push_heap{};
    inline constexpr __algo::__pop_heap pop_heap{};
    inline constexpr __algo::__is_heap is_heap{};
}  // namespace tay
