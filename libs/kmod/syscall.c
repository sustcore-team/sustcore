/**
 * @file syscall.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用实现
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <kmod/syscall.h>

void exit(int code) {
    // TODO: 实现退出系统调用
    while (true);
}

void *mapmem(Capability cap) {
    // TODO: 实现映射内存系统调用
    return nullptr;
}