/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtio块设备驱动主文件
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <sus/bits.h>

int kmod_main(void) {
    asm volatile(
        "mv a7, %0\n"
        "mv a0, %1\n"
        "ecall\n"
        :
        : "r"(93), "r"(0)
        : "a0", "a7");
    while (true);
    return 0;
}