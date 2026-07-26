/**
 * @file attributes.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 属性定义
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