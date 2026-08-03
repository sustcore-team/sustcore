/**
 * @file stdlib.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 freestanding C 环境提供与标准 <stdlib.h> 对应的基础工具声明。
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

#include <limits.h>
#include <stddef.h>
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
