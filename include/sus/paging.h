/**
 * @file paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 抽象页表接口
 * @version alpha-1.0.0
 * @date 2025-11-22
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <stddef.h>
#include <sus/arch.h>
#include <sus/bits.h>

#define PAGE_SIZE (4096)

#if __ARCHITECTURE__ == riscv64

#include <arch/riscv64/paging.h>

#endif