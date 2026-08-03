/**
 * @file range.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 基准测试 Tay 区间操作的性能。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/algobase.h>
#include <tay/bits.h>
#include <tay/range.h>

#include <chrono>
#include <cstdio>

int main() {
    constexpr u64_t iterations = 10000000;
    auto start                 = std::chrono::steady_clock::now();
    const auto seed            = static_cast<u64_t>(start.time_since_epoch().count());
    volatile u64_t checksum    = 0;
    for (u64_t i = 0; i < iterations; ++i) {
        const auto value = i + seed;
        tay::range<u64_t> a{value, value + 32};
        tay::range<u64_t> b{value + 8, value + 48};
        auto overlap = tay::intersection(a, b);
        checksum     = checksum + overlap.size() + tay::clamp(value, value / 2, value + 1);
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    std::printf(
        "taycpplib range: iterations=%llu elapsed=%lld ns ns/op=%.2f "
        "checksum=%llu\n",
        (unsigned long long)iterations, (long long)elapsed, (double)elapsed / (double)iterations,
        (unsigned long long)checksum);
    return 0;
}
