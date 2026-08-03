/**
 * @file wchar.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 freestanding C 环境提供与标准 <wchar.h> 对应的宽字符类型和操作声明。
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

#include <bits/types/mbstate_t.h>

typedef int wint_t;

#ifdef __cplusplus
}
#undef restrict
#endif