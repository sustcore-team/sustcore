/**
 * @file bits.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay C 库使用的定宽整数类型别名。
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

#include <stddef.h>
#include <stdint.h>

#if __SIZEOF_POINTER__ == 4
#define XLEN 32
#elif __SIZEOF_POINTER__ == 8
#define XLEN 64
#endif

// i8_t, i16_t, i32_t, i64_t, u8_t, u16_t, u32_t, u64_t
typedef int8_t i8_t;
typedef int16_t i16_t;
typedef int32_t i32_t;
typedef int64_t i64_t;
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

/** 字节 */
typedef u8_t byte;
/** 字 */
typedef u16_t word;
/** 双字 */
typedef u32_t dword;
/** 四字 */
typedef u64_t qword;

#define BYTE_MAX  ((byte)0xFF)
#define WORD_MAX  ((word)0xFFFF)
#define DWORD_MAX ((dword)0xFFFFFFFF)
#define QWORD_MAX ((qword)0xFFFFFFFFFFFFFFFF)

#if XLEN == 32
typedef u32_t xlen_t;
typedef i32_t slen_t;
#elif XLEN == 64
typedef u64_t xlen_t;
typedef i64_t slen_t;
#else
#error "Unsupported XLEN"
#endif

typedef slen_t off_t;
typedef slen_t ssize_t;
typedef xlen_t addr_t;

#ifdef __cplusplus
}
#undef restrict
#endif
