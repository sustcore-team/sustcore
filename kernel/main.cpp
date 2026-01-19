/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cstring>

#include <mem/alloc.h>
#include <arch/trait.h>

void post_init(void) {
}

void init(void) {
}

void kernel_setup(void) {
    ArchSerial::serial_write_string("欢迎使用 Sustcore Riscv64 内核!\n");
    ArchInitialization::pre_init();
    while (true);
}