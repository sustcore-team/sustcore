/**
 * @file introsort.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Introsort algorithm implementation.
 * @version 0.1.0-dev.1
 * @date 2026-07-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <functional>
#include <iterator>
#include <utility>

namespace tay::detail {
    namespace __introsort {
        // the introsort works best when N = 32
        inline constexpr auto insertion_sort_threshold = 32;

        template <typename Compare, typename Proj, typename Left,
                  typename Right>
        [[nodiscard]]
        constexpr bool projcmp(Compare& comp, Proj& proj, Left&& left,
                               Right&& right) {
            return std::invoke(comp,
                               std::invoke(proj, std::forward<Left>(left)),
                               std::invoke(proj, std::forward<Right>(right)));
        }

        template <typename RandomIt, typename Compare, typename Proj>
            requires std::random_access_iterator<RandomIt> &&
                     std::sortable<RandomIt, Compare, Proj>
        constexpr void insertion_sort(RandomIt first, RandomIt last,
                                      Compare& comp, Proj& proj) {
            if (first == last) {
                return;
            }

            for (RandomIt current = first + 1; current != last; ++current) {
                std::iter_value_t<RandomIt> value(
                    std::ranges::iter_move(current));
                RandomIt hole = current;

                while (hole != first && projcmp(comp, proj, value, *(hole - 1)))
                {
                    *hole = std::ranges::iter_move(hole - 1);
                    --hole;
                }
                *hole = std::move(value);
            }
        }

        template <typename RandomIt, typename Compare, typename Proj>
            requires std::random_access_iterator<RandomIt> &&
                     std::sortable<RandomIt, Compare, Proj>
        constexpr void sift_down(RandomIt first,
                                 std::iter_difference_t<RandomIt> root,
                                 std::iter_difference_t<RandomIt> count,
                                 Compare& comp, Proj& proj) {
            using difference_type = std::iter_difference_t<RandomIt>;

            while (root < count / 2) {
                difference_type child = root * 2 + 1;
                if (child + 1 < count &&
                    projcmp(comp, proj, *(first + child), *(first + child + 1)))
                {
                    ++child;
                }

                if (!projcmp(comp, proj, *(first + root), *(first + child))) {
                    return;
                }

                std::ranges::iter_swap(first + root, first + child);
                root = child;
            }
        }

        template <typename RandomIt, typename Compare, typename Proj>
            requires std::random_access_iterator<RandomIt> &&
                     std::sortable<RandomIt, Compare, Proj>
        constexpr void heapsort(RandomIt first, RandomIt last, Compare& comp,
                                Proj& proj) {
            using difference_type = std::iter_difference_t<RandomIt>;

            if (first == last) {
                return;
            }

            const difference_type count = last - first;
            if (count < 2) {
                return;
            }

            for (difference_type root = count / 2; root > 0;) {
                --root;
                sift_down(first, root, count, comp, proj);
            }

            for (difference_type remaining = count; remaining > 1;) {
                --remaining;
                std::ranges::iter_swap(first, first + remaining);
                sift_down(first, difference_type{0}, remaining, comp, proj);
            }
        }

        template <typename RandomIt, typename Compare, typename Proj>
            requires std::random_access_iterator<RandomIt> &&
                     std::sortable<RandomIt, Compare, Proj>
        [[nodiscard]]
        constexpr RandomIt partition(RandomIt first, RandomIt last,
                                     Compare& comp, Proj& proj) {
            RandomIt middle     = first + (last - first) / 2;
            RandomIt last_value = last - 1;

            if (projcmp(comp, proj, *middle, *first)) {
                std::ranges::iter_swap(middle, first);
            }
            if (projcmp(comp, proj, *last_value, *middle)) {
                std::ranges::iter_swap(last_value, middle);
            }
            if (projcmp(comp, proj, *middle, *first)) {
                std::ranges::iter_swap(middle, first);
            }

            RandomIt pivot = last - 2;
            std::ranges::iter_swap(middle, pivot);

            RandomIt left  = first;
            RandomIt right = pivot;
            while (true) {
                do {
                    ++left;
                } while (projcmp(comp, proj, *left, *pivot));

                do {
                    --right;
                } while (projcmp(comp, proj, *pivot, *right));

                if (left >= right) {
                    break;
                }
                std::ranges::iter_swap(left, right);
            }

            std::ranges::iter_swap(left, pivot);
            return left;
        }

        template <typename RandomIt, typename Compare, typename Proj>
            requires std::random_access_iterator<RandomIt> &&
                     std::sortable<RandomIt, Compare, Proj>
        constexpr void introsort_loop(
            RandomIt first, RandomIt last,
            std::iter_difference_t<RandomIt> depth_limit, Compare& comp,
            Proj& proj) {
            using difference_type = std::iter_difference_t<RandomIt>;

            while (last - first > insertion_sort_threshold) {
                if (depth_limit == 0) {
                    heapsort(first, last, comp, proj);
                    return;
                }
                --depth_limit;

                RandomIt pivot = partition(first, last, comp, proj);
                const difference_type left_size  = pivot - first;
                const difference_type right_size = last - (pivot + 1);

                if (left_size < right_size) {
                    introsort_loop(first, pivot, depth_limit, comp, proj);
                    first = pivot + 1;
                } else {
                    introsort_loop(pivot + 1, last, depth_limit, comp, proj);
                    last = pivot;
                }
            }

            insertion_sort(first, last, comp, proj);
        }

        template <typename RandomIt, typename Compare, typename Proj>
            requires std::random_access_iterator<RandomIt> &&
                     std::sortable<RandomIt, Compare, Proj>
        constexpr void introsort(RandomIt first, RandomIt last, Compare comp,
                                 Proj proj) {
            using difference_type = std::iter_difference_t<RandomIt>;

            if (first == last) {
                return;
            }

            difference_type depth_limit = 0;
            for (difference_type count = last - first; count > 1; count /= 2) {
                ++depth_limit;
            }

            introsort_loop(first, last, depth_limit * 2, comp, proj);
        }
    }  // namespace __introsort

    template <typename RandomIt, typename Compare, typename Proj>
    constexpr void introsort(RandomIt first, RandomIt last, Compare comp,
                             Proj proj)
        requires std::random_access_iterator<RandomIt> &&
                 std::sortable<RandomIt, Compare, Proj>
    {
        __introsort::introsort(first, last, std::move(comp), std::move(proj));
    }
}  // namespace tay::detail
