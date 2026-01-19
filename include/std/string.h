/**
 * @file string.h
 * @author theflysong (song_of_the_fly@163.com) Yoyoooo
 * @brief string.h
 * @version alpha-1.0.0
 * @date 2022-12-31
 *
 * @copyright Copyright (c) 2022 TayhuangOS Development Team
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#pragma once

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

#include <stddef.h>

/**
 * @brief 设置dst内存区域的前size个字节为val
 *
 * @param dst 目标内存地址
 * @param val 要设置的值
 * @param size 要设置的字节数
 */
void memset(void *restrict dst, int val, size_t size);

/**
 * @brief 复制src内存区域的前size个字节到dst内存区域
 *
 * @param dst 目标内存地址
 * @param src 源内存地址
 * @param size 要复制的字节数
 */
void memcpy(void *restrict dst, const void *restrict src, size_t size);

/**
 * @brief 比较字符串
 *
 * @param str1 字符串1
 * @param str2 字符串2
 * @return int 按照字典序返回比较结果:
 *              -1: str1 < str2
 *               0: str1 == str2
 *               1: str1 > str2
 */
int strcmp(const char *restrict str1, const char *restrict str2);

/**
 * @brief 计算字符串长度
 *
 * @param str 字符串
 * @return int 字符串长度
 */
int strlen(const char *restrict str);

/**
 * @brief 计算字符串长度 (<= count)
 *
 * @param str 字符串
 * @return int 字符串长度
 */
int strnlen(const char *restrict str, int count);

/**
 * @brief 复制字符串
 *
 * @param dst 目标字符串
 * @param src 源字符串
 * @return char* 目标字符串
 */
char *strcpy(void *restrict dst, const char *restrict src);

/**
 * @brief 查找字符串中首次出现的字符
 *
 * @param str 字符串
 * @param ch 要查找的字符
 * @return char* 指向首次出现字符的指针，若未找到则返回NULL
 */
char *strchr(const char *restrict str, char ch);

/**
 * @brief 比较字符串(前count个字符)
 *
 * @param str1 字符串1
 * @param str2 字符串2
 * @param count 比较的字符数
 * @return int 按照字典序返回比较结果:
 *              -1: str1 < str2
 *               0: str1 == str2
 *               1: str1 > str2
 */
int strncmp(const char *restrict str1, const char *restrict str2, int count);

/**
 * @brief 复制字符串(前count个字符)
 *
 * @param dst 目标字符串
 * @param src 源字符串
 * @param count 复制的字符数
 * @return char* 目标字符串
 */
char *strncpy(char *restrict dst, const char *restrict src, int count);

/**
 * @brief 查找字符串中末次出现的字符
 *
 * @param str 字符串
 * @param ch 要查找的字符
 * @return char* 指向末次出现字符的指针，若未找到则返回NULL
 */
char *strrchr(const char *restrict str, char ch);

/**
 * @brief 计算字符串中连续包含accept中字符的长度
 *
 * @param str 字符串
 * @param accept 接受的字符集合
 * @return int 连续包含的字符数
 */
int strspn(const char *restrict str, const char *restrict accept);

/**
 * @brief 计算字符串中连续不包含accept中字符的长度
 *
 * @param str 字符串
 * @param accept 不接受的字符集合
 * @return int 连续不包含的字符数
 */
int strcspn(const char *restrict str, const char *restrict accept);

/**
 * @brief 查找字符串中首次出现accept中任一字符的位置
 *
 * @param str 字符串
 * @param accept 接受的字符集合
 * @return char* 指向首次出现字符的指针，若未找到则返回NULL
 */
char *strpbrk(const char *restrict str, const char *restrict accept);

/**
 * @brief 将src拼接在dst上
 *
 * @param dst 目标字符串
 * @param src 源字符串
 * @return char* 目标字符串
 */
char *strcat(char *restrict dst, const char *restrict src);

/**
 * @brief 将src的前count个字符拼接在dst上
 *
 * @param dst 目标字符串
 * @param src 源字符串
 * @param count 拼接的字符数
 * @return char* 目标字符串
 */
char *strncat(char *dst, const char *src, int count);

/**
 * @brief 分割字符串
 *
 * @param str 字符串
 * @param split 分隔符字符串
 * @return char* 指向分割后子字符串的指针，若无更多子字符串则返回NULL
 */
char *strtok(char *restrict str, const char *restrict split);

/**
 * @brief 从src中复制size个字节到dst中，可内存重叠情况
 *
 * @param dst 目标内存地址
 * @param src 源内存地址
 * @param size 复制的字节数
 * @return void* 目标内存地址
 */
void *memmove(void *dst, const void *src, size_t size);

/**
 * @brief 比较内存中前count个字节
 *
 * @param str1 内存区域1
 * @param str2 内存区域2
 * @param count 比较的字节数
 * @return int  按照字典序返回比较结果:
 *              -1: str1 < str2
 *               0: str1 == str2
 *               1: str1 > str2
 */
int memcmp(const void *restrict str1, const void *restrict str2, int count);

/**
 * @brief 在内存的前count个字节中查找ch
 *
 * @param str 内存区域
 * @param ch 要查找的字符
 * @param count 查找的字节数
 * @return void* 指向首次出现字符的指针，若未找到则返回NULL
 */
void *memchr(const void *restrict str, char ch, int count);

#ifdef __cplusplus
}
#undef restrict
#endif