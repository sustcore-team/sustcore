/**
 * @file types.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 各种类型
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/** 字节 */
typedef uint8_t byte;
/** 字 */
typedef uint16_t word;
/** 双字 */
typedef uint32_t dword;
/** 四字 */
typedef uint64_t qword;

typedef int8_t i8_t;
typedef int16_t i16_t;
typedef int32_t i32_t;
typedef int64_t i64_t;
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

typedef u64_t xlen_t;
typedef i64_t slen_t;

typedef slen_t off_t;
typedef slen_t ssize_t;
typedef xlen_t addr_t;

// 逻辑运算符 a=>b 的实现
// a=>b 即为 (!a | b) 相当于 (a & b) == b
#define BOOL_IMPLIES(a, b) ((!(a)) | (b))

// 逐位计算的话, 则其相当于 (y & x) == y
#define BITS_IMPLIES(x, y) (((x) & (y)) == (y))

#define QWORD_MAX ((qword)0xFFFFFFFFFFFFFFFF)
#define DWORD_MAX ((dword)0xFFFFFFFF)
#define WORD_MAX  ((word)0xFFFF)
#define BYTE_MAX  ((byte)0xFF)

// Attributes
#define PACKED     __attribute__((packed))
#define NAKED      __attribute__((naked))
#define ALIGNED(x) __attribute__((aligned(x)))
#define SECTION(x) __attribute__((section(x)))

#ifdef __cplusplus
}
#endif