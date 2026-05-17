/**
 * @file printf.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief printf function
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/syscall.h>
#include <prm.h>
#include <sus/baseio.h>

#include <cstdio>
#include <cstring>

extern "C" {
int kputs(const char *str) {
    size_t len = strlen(str);
    sys_write_serial(str, len);
    return len;
}

int printf(const char *format, ...) {
    // 简单实现一个简单的printf
    char buf[256];
    va_list args;
    va_start(args, format);
    int len = vsprintf(buf, format, args);
    va_end(args);
    kputs(buf);
    return len;
}
}