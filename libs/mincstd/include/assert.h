/**
 * @file assert.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 freestanding C 环境提供与标准 <assert.h> 对应的断言接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#pragma once

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

/**
 * @brief 断言
 *
 * @param expression 表达式
 * @param file 文件
 * @param base_file 源文件
 * @param line 行
 */
void assertion_failure(const char *expression, const char *file, const char *base_file, int line);
/**
 * @brief 崩溃断言
 *
 * @param expression 表达式
 * @param file 文件
 * @param base_file 源文件
 * @param line 行
 */
void panic_failure(const char *expression, const char *file, const char *base_file, int line);
/**
 * @brief 崩溃
 *
 * @param format 格式化字符串
 * @param ... 可变参数
 */

#ifdef __cplusplus
[[noreturn]]
#endif
void panic(const char *format, ...);

#ifndef ASSERT_IMPLEMENTED
#define ASSERT_IMPLEMENTED 0
#endif

// 不开启DEBUG
#if defined(NDEBUG) || !ASSERT_IMPLEMENTED
/** 断言 */
#define assert(expression)       ((void)(expression))
/** 崩溃断言 */
#define panic_assert(expression) ((void)(expression))
#else

#define assert(expression) \
    if (!(expression))     \
    assertion_failure(#expression, __FILE__, __BASE_FILE__, __LINE__)
/** 崩溃断言 */
#define panic_assert(expression) \
    if (!(expression))           \
    panic_failure(#expression, __FILE__, __BASE_FILE__, __LINE__)

#endif

#ifdef __cplusplus
}
#undef restrict
#endif
