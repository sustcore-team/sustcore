/**
 * @file sections.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动期可回收段属性
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define BOOT_INIT_TEXT   __attribute__((section(".init.text")))
#define BOOT_INIT_DATA   __attribute__((section(".init.data")))
#define BOOT_INIT_RODATA __attribute__((section(".init.rodata")))
#define BOOT_INIT_BSS    __attribute__((section(".init.bss")))
