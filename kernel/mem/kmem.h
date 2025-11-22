/**
 * @file kmem.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核内存相关
 * @version alpha-1.0.0
 * @date 2025-11-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stddef.h>

#define KERNEL_VA_OFFSET (size_t)(0xFFFF'FFFF'0000'0000ULL)

#define KA2PA(ka) ((void *)((size_t)(ka) - KERNEL_VA_OFFSET))
#define PA2KA(pa) ((void *)((size_t)(pa) + KERNEL_VA_OFFSET))