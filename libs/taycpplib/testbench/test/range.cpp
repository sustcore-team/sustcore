/**
 * @file range.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::range 的边界、包含和交集语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/range.h>

int main() {
    constexpr tay::range<int> outer{1, 8};
    constexpr tay::range<int> inner{3, 6};
    static_assert(outer.size() == 7 && tay::within(outer, inner));
    static_assert(tay::intersection(outer, tay::range<int>{5, 10}) == tay::range<int>{5, 8});

    return 0;
}
