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

#define STDIO_CHAN (8)

int basec_puts(iochan_t chan, const char *str) {
    if (chan == STDIO_CHAN) {
        // 标准io通道, 使用sys_write_syscall
        puts(str);
        return 0;
    }
    // 未知通道
    return -1;
}