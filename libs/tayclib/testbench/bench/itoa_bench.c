/**
 * @file itoa_bench.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 基准测试 Tay C 库整数到字符串转换的性能。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <tay/bits.h>
#include <tay/itoa.h>
#include <time.h>

static u64_t now_ns(void) {
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (u64_t)value.tv_sec * 1000000000ull + (u64_t)value.tv_nsec;
}

int main(void) {
    const unsigned long long iterations = 2000000;
    char buffer[64];
    unsigned long long checksum = 0;
    u64_t start                 = now_ns();
    for (unsigned long long i = 0; i < iterations; ++i) {
        checksum += (unsigned char)lltoa_s((long long)i - 1000000, buffer, sizeof(buffer), 10)[0];
        checksum += (unsigned char)ulltoa_s(i * 2654435761ull, buffer, sizeof(buffer), 16)[0];
    }
    u64_t elapsed = now_ns() - start;
    printf("tayclib itoa: iterations=%llu elapsed=%llu ns ns/op=%.2f checksum=%llu\n",
           iterations * 2, (unsigned long long)elapsed, (double)elapsed / (double)(iterations * 2),
           checksum);
    return 0;
}
