/**
 * @file introsort.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现内省排序算法的内部细节。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/utility.h>

#include <functional>
#include <iterator>
#include <utility>

namespace tay::detail {
    namespace __introsort {
        // the introsort works best when N = 32
        inline constexpr auto insertion_sort_threshold = 32;

        template <typename RandomIt, typename Compare>
            requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare>
        constexpr void insertion_sort(RandomIt first, RandomIt last, Compare& comp) {
            if (first == last) {
                return;
            }

            for (RandomIt current = first + 1; current != last; ++current) {
                std::iter_value_t<RandomIt> value(std::ranges::iter_move(current));
                RandomIt hole = current;

                while (hole != first && comp(value, *(hole - 1))) {
                    *hole = std::ranges::iter_move(hole - 1);
                    --hole;
                }
                *hole = std::move(value);
            }
        }

        template <typename RandomIt, typename Compare>
            requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare>
        constexpr void sift_down(RandomIt first, std::iter_difference_t<RandomIt> root,
                                 std::iter_difference_t<RandomIt> count, Compare& comp) {
            using difference_type = std::iter_difference_t<RandomIt>;

            while (root < count / 2) {
                difference_type child = root * 2 + 1;
                if (child + 1 < count && comp(*(first + child), *(first + child + 1))) {
                    ++child;
                }

                if (!comp(*(first + root), *(first + child))) {
                    return;
                }

                std::ranges::iter_swap(first + root, first + child);
                root = child;
            }
        }

        template <typename RandomIt, typename Compare>
            requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare>
        constexpr void heapsort(RandomIt first, RandomIt last, Compare& comp) {
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
                sift_down(first, root, count, comp);
            }

            for (difference_type remaining = count; remaining > 1;) {
                --remaining;
                std::ranges::iter_swap(first, first + remaining);
                sift_down(first, difference_type{0}, remaining, comp);
            }
        }

        template <typename RandomIt, typename Compare>
            requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare>
        [[nodiscard]]
        constexpr RandomIt partition(RandomIt first, RandomIt last, Compare& comp) {
            RandomIt middle     = first + (last - first) / 2;
            RandomIt last_value = last - 1;

            if (comp(*middle, *first)) {
                std::ranges::iter_swap(middle, first);
            }
            if (comp(*last_value, *middle)) {
                std::ranges::iter_swap(last_value, middle);
            }
            if (comp(*middle, *first)) {
                std::ranges::iter_swap(middle, first);
            }

            RandomIt pivot = last - 2;
            std::ranges::iter_swap(middle, pivot);

            RandomIt left  = first;
            RandomIt right = pivot;
            while (true) {
                do {
                    ++left;
                } while (comp(*left, *pivot));

                do {
                    --right;
                } while (comp(*pivot, *right));

                if (left >= right) {
                    break;
                }
                std::ranges::iter_swap(left, right);
            }

            std::ranges::iter_swap(left, pivot);
            return left;
        }

        template <typename RandomIt, typename Compare>
            requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare>
        constexpr void introsort_loop(RandomIt first, RandomIt last,
                                      std::iter_difference_t<RandomIt> depth_limit, Compare& comp) {
            using difference_type = std::iter_difference_t<RandomIt>;

            while (last - first > insertion_sort_threshold) {
                if (depth_limit == 0) {
                    heapsort(first, last, comp);
                    return;
                }
                --depth_limit;

                RandomIt pivot                   = __introsort::partition(first, last, comp);
                const difference_type left_size  = pivot - first;
                const difference_type right_size = last - (pivot + 1);

                if (left_size < right_size) {
                    introsort_loop(first, pivot, depth_limit, comp);
                    first = pivot + 1;
                } else {
                    introsort_loop(pivot + 1, last, depth_limit, comp);
                    last = pivot;
                }
            }

            insertion_sort(first, last, comp);
        }

        template <typename RandomIt, typename Compare>
            requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare>
        constexpr void introsort(RandomIt first, RandomIt last, Compare comp) {
            using difference_type = std::iter_difference_t<RandomIt>;

            if (first == last) {
                return;
            }

            difference_type depth_limit = 0;
            for (difference_type count = last - first; count > 1; count /= 2) {
                ++depth_limit;
            }

            introsort_loop(first, last, depth_limit * 2, comp);
        }
    }  // namespace __introsort

    template <typename RandomIt, typename Compare, typename Proj>
    constexpr void introsort(RandomIt first, RandomIt last, Compare comp, Proj proj)
        requires std::random_access_iterator<RandomIt> && std::sortable<RandomIt, Compare, Proj>
    {
        using comparator = projected_compare<Compare, Proj>;
        comparator combined(std::move(comp), std::move(proj));
        __introsort::introsort(first, last, std::move(combined));
    }
}  // namespace tay::detail
