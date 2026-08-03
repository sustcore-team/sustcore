/**
 * @file sort.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 排序算法的有序性和边界行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/algo/sort.h>

#include <cassert>
#include <cstddef>

namespace {
    template <typename T, size_t Size>
    constexpr bool equal(const T (&left)[Size], const T (&right)[Size]) {
        for (size_t index = 0; index < Size; ++index) {
            if (!(left[index] == right[index])) {
                return false;
            }
        }
        return true;
    }

    struct record {
        int key;
        int payload;

        constexpr bool operator==(const record&) const = default;
    };

    struct move_only {
        int value;

        constexpr explicit move_only(int value) : value(value) {}
        constexpr move_only(const move_only&)            = delete;
        constexpr move_only& operator=(const move_only&) = delete;
        constexpr move_only(move_only&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        constexpr move_only& operator=(move_only&& other) noexcept {
            value       = other.value;
            other.value = -1;
            return *this;
        }
    };

    struct move_only_less {
        constexpr bool operator()(const move_only& left, const move_only& right) const {
            return left.value < right.value;
        }
    };

    constexpr bool constexpr_sort_works() {
        int values[]   = {5, 1, 4, 1, 3, 2};
        int expected[] = {1, 1, 2, 3, 4, 5};
        auto result    = tay::sort(values);
        return result == values + 6 && equal(values, expected);
    }
}  // namespace

static_assert(std::sortable<int*>);
static_assert(std::sortable<record*, std::ranges::less, decltype(&record::key)>);
static_assert(std::sortable<move_only*, move_only_less>);
static_assert(!std::permutable<const int*>);
static_assert(!std::sortable<const int*>);
static_assert(constexpr_sort_works());

int main() {
    int* empty = nullptr;
    assert(tay::sort(empty, empty) == empty);

    int descending[] = {1, 4, 2, 5, 3};
    auto descending_end =
        tay::sort(descending, descending + 5, [](int left, int right) { return left > right; });
    const int expected_descending[] = {5, 4, 3, 2, 1};
    assert(descending_end == descending + 5);
    assert(equal(descending, expected_descending));

    record records[] = {{3, 30}, {1, 10}, {2, 20}, {1, 11}};
    tay::sort(records, {}, &record::key);
    const record expected_records[] = {
        {1, 10},
        {1, 11},
        {2, 20},
        {3, 30},
    };
    assert(equal(records, expected_records));

    move_only objects[] = {move_only(4), move_only(2), move_only(3), move_only(1)};
    tay::sort(objects, [](const move_only& left, const move_only& right) {
        return left.value < right.value;
    });
    for (size_t index = 0; index < 4; ++index) {
        assert(objects[index].value == static_cast<int>(index + 1));
    }

    return 0;
}
