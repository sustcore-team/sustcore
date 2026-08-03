/**
 * @file range.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::range 的交集和包含操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/range.h>

#include <cstdio>

int main() {
    constexpr tay::range<int> available{10, 30};
    constexpr tay::range<int> requested{20, 40};
    constexpr auto overlap = tay::intersection(available, requested);

    std::printf("available: [%d, %d)\n", available.begin, available.end);
    std::printf("requested: [%d, %d)\n", requested.begin, requested.end);
    std::printf("overlap: [%d, %d), size=%zu\n", overlap.begin, overlap.end, overlap.size());
    std::printf("25 is available: %s\n", tay::within(available, 25) ? "yes" : "no");
    return 0;
}
