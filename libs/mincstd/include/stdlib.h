/**
 * @file stdlib.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief stdlib.h
 * @version 1.0.0
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

#include <stddef.h>
#include <limits.h>
#include <stdint.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
unsigned long int strtoul(const char *restrict str, char **endptr, int base);

#ifdef __cplusplus
}
#undef restrict
#endif
