/**
 * @file bits.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 不同位长的bit
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#pragma once

#include <stdint.h>

/** 字节 */
typedef uint8_t byte;
/** 字 */
typedef uint16_t word;
/** 双字 */
typedef uint32_t dword;
/** 四字 */
typedef uint64_t qword;

/** 8位 */
typedef uint8_t b8;
/** 16位 */
typedef uint16_t b16;
/** 32位 */
typedef uint32_t b32;
/** 64位 */
typedef uint64_t b64;

// 获取机器位数
#if BITS == 16
/** 机器位数 */
typedef b16 machine_bits;
typedef int16_t signed_machine_bits;
#elif BITS == 32
/** 机器位数 */
typedef b32 machine_bits;
typedef int32_t signed_machine_bits;
#elif BITS == 64
/** 机器位数 */
typedef b64 machine_bits;
typedef int64_t signed_machine_bits;
#else
/** 机器位数 */
typedef b64 machine_bits;
typedef int64_t signed_machine_bits;
#endif

/** 机器位数 */
typedef machine_bits umb_t;
typedef signed_machine_bits smb_t;

// 逻辑运算符 a=>b 的实现
// a=>b 即为 (!a | b)
#define BOOL_IMPLIES(a, b) ((!(a)) | (b))

// 逐位计算的话, 则其相当于 (y & x) == y
#define BITS_IMPLIES(x, y) (((x) & (y)) == (y))

#define QWORD_MAX (0xFFFF'FFFF'FFFF'FFFF)
#define DWORD_MAX (0xFFFF'FFFF)
#define WORD_MAX  (0xFFFF)
#define BYTE_MAX  (0xFF)