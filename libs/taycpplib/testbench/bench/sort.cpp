/**
 * @file sort.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 基准测试 Tay 排序算法的性能。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/algo/sort.h>
#include <tay/bits.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <iostream>

namespace {
    constexpr size_t batch_size = 256;

    template <size_t Size>
    void benchmark(const char* name, u64_t iterations, u64_t seed) {
        volatile u64_t checksum = 0;
        u64_t state             = seed;
        i64_t elapsed           = 0;

        // 堆上分配连续内存（推荐使用 unique_ptr，零额外开销）
        auto values = std::make_unique<u32_t[]>(batch_size * Size);

        for (u64_t offset = 0; offset < iterations; offset += batch_size) {
            const auto remaining = iterations - offset;
            const auto count     = remaining < batch_size ? remaining : batch_size;

            // 生成随机数据
            for (size_t item = 0; item < count; ++item) {
                for (size_t index = 0; index < Size; ++index) {
                    state                       = state * 6364136223846793005ULL + 1;
                    values[item * Size + index] = static_cast<u32_t>(state >> 32);
                }
            }

            const auto start = std::chrono::steady_clock::now();
            // 排序：传入起始指针和结尾指针（符合 random_access_iterator 概念）
            for (size_t item = 0; item < count; ++item) {
                auto* first = values.get() + item * Size;
                auto* last  = first + Size;
                tay::sort(first, last);  // 使用迭代器版本
            }
            elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();

            // 校验和（取每行首尾元素）
            for (size_t item = 0; item < count; ++item) {
                const auto base = item * Size;
                checksum        = checksum + values[base] + values[base + Size - 1];
            }
        }
        std::cout << std::format(
            "{:28} size={:6d} iterations={:8d} elapsed={:11d} ns "
            "ns/sort={:12.2f} ns/element={:8.3f} checksum={}\n",
            name, (unsigned long long)Size, (unsigned long long)iterations, (long long)elapsed,
            (double)elapsed / (double)iterations, (double)elapsed / (double)(iterations * Size),
            (unsigned long long)checksum);
    }
}  // namespace

int main() {
    const auto seed =
        static_cast<u64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

    std::printf("taycpplib sort benchmark\n");
    benchmark<8>("sort-8", 1000000, seed);
    benchmark<32>("sort-32", 200000, seed);
    benchmark<128>("sort-128", 20000, seed);
    benchmark<1024>("sort-1024", 5000, seed);
    benchmark<8192>("sort-8192", 500, seed);
    benchmark<65536>("sort-65536", 10, seed);

    return 0;
}
