/**
 * @file stdlib.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief stdlib.h
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

#include <limits.h>
#include <stdint.h>

unsigned long int strtoul(const char *restrict str, char **endptr, int base);

#ifdef __cplusplus
}
#undef restrict
#endif