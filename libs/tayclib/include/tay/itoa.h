/**
 * @file itoa.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief integer to ascii conversion
 * @version 0.1.0-dev.1
 * @date 2026-07-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

#include <stddef.h>
#include <tay/bits.h>

/**
 * @brief 正整数转字符串(safe version)
 *
 * 在以下情况, itoa_s将不会进行任何操作, 直接返回buf:
 * 1. buf 为 NULL
 * 2. bufsz 为 0
 * 3. radix 小于 2 或大于 36
 *
 * 当 buf 大小不足以存储转换后的字符串时, itoa_s将会截断字符串, 并在最后一个字节处添加'\0'
 * 例如: itoa_s(123456, buf, 4, 10) 将会在buf中存储 "123\0"
 * 而 itoa_s(-12, buf, 2, 10) 将会在buf中存储 "-\0"
 *
 * 该实现的时间复杂度为 O(log(val)), 空间复杂度为 O(1)
 *
 * @param val 要转换的整数值
 * @param buf 存储转换后字符串的缓冲区
 * @param bufsz 缓冲区大小
 * @param radix 进制, 范围为2~36
 * @return char* 转换后的字符串, 如果buf为NULL或bufsz为0, 则返回NULL
 */
char *itoa_s(int val, char *buf, size_t bufsz, int radix);
char *utoa_s(unsigned int val, char *buf, size_t bufsz, int radix);
char *ltoa_s(long val, char *buf, size_t bufsz, int radix);
char *ultoa_s(unsigned long val, char *buf, size_t bufsz, int radix);
char *lltoa_s(long long val, char *buf, size_t bufsz, int radix);
char *ulltoa_s(unsigned long long val, char *buf, size_t bufsz, int radix);

#ifdef __cplusplus
}
#undef restrict
#endif