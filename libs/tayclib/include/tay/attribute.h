/**
 * @file attribute.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay C 库使用的编译器属性宏。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

// Attributes
#define PACKED            __attribute__((packed))
#define NAKED             __attribute__((naked))
#define ALIGNED(x)        __attribute__((aligned(x)))
#define SECTION(x)        __attribute__((section(x)))
#define __ALWAYS_INLINE__ __attribute__((always_inline))
#define ALWAYS_INLINE     inline __ALWAYS_INLINE__

#ifdef __cplusplus
}
#undef restrict
#endif