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

/**
 * @brief 内核虚拟地址偏移
 *
 */
#define KERNEL_VA_OFFSET (size_t)(0xFFFF'FFFF'0000'0000ULL)

#define KA2PA(ka) ((void *)((size_t)(ka) - KERNEL_VA_OFFSET))
#define PA2KA(pa) ((void *)((size_t)(pa) + KERNEL_VA_OFFSET))

/**
 * @brief 内核物理内存虚拟地址偏移
 *
 * @note 与KERNEL_VA_OFFSET不同之处在于, 该偏移用于内核堆.
 *
 */
#define KHEAP_VA_OFFSET (size_t)(0xFFFF'FFC0'0000'0000ULL)

#define KHEAPA2PA(ka) ((void *)((size_t)(ka) - KHEAP_VA_OFFSET))
#define PA2KHEAPA(pa) ((void *)((size_t)(pa) + KHEAP_VA_OFFSET))