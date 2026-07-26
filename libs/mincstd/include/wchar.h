/**
 * @file wchar.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief wchar
 * @version 0.1.0-dev.1
 * @date 2026-03-01
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