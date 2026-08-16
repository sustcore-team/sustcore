/**
 * @file sort_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 排序算法可在 freestanding 环境中编译。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/algo/sort.h>
#include <tay/utility.h>

#include <functional>

namespace {
    struct record {
        int key;
        int payload;
    };

    using record_key_less = tay::projected_compare<std::ranges::less, int record::*>;

    constexpr bool sort_works() {
        int values[] = {4, 1, 3, 2};
        if (tay::sort(values) != values + 4) {
            return false;
        }
        for (int index = 0; index < 4; ++index) {
            if (values[index] != index + 1) {
                return false;
            }
        }

        record records[] = {{2, 20}, {1, 10}, {3, 30}};
        tay::sort(records, {}, &record::key);
        return records[0].key == 1 && records[1].key == 2 && records[2].key == 3;
    }
}  // namespace

static_assert(std::sortable<int *>);
static_assert(std::sortable<record *, std::ranges::less, decltype(&record::key)>);
static_assert(!std::sortable<const int *>);
static_assert(record_key_less(std::ranges::less{}, &record::key)(record{.key = 1},
                                                                 record{.key = 2}));
static_assert(sort_works());

int main() {
    return sort_works() ? 0 : 1;
}
