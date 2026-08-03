/**
 * @file bool.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay C 库的布尔值类型和逻辑操作。
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

#include <stdbool.h>

// per bit implication
#define _IMPLIES(a, b) (((a) & (b)) == (b))
#define _NOR(a, b)     (!((a) | (b)))
#define _NAND(a, b)    (!((a) & (b)))
#define _XNOR(a, b)    (!((a) ^ (b)))

#ifdef __cplusplus
}
#undef restrict
#endif