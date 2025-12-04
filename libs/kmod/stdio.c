/**
 * @file stdio.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 标准输入输出库实现
 * @version alpha-1.0.0
 * @date 2025-12-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <basec/baseio.h>
#include <stdio.h>

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vbprintf(puts, fmt, args);
    va_end(args);
    return ret;
}