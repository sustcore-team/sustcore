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
#include <sus/attributes.h>

extern bool post_init_flag;

/**
 * @brief 内核虚拟地址偏移
 * 60'0000'0000 => 7F'FFFF'FFFF (128GB)
 */
#define KERNEL_VA_OFFSET (size_t)(0xFFFF'FFFF'0000'0000ULL)

#define KA2PA(ka) ((void *)((size_t)(ka) - KERNEL_VA_OFFSET))
#define PA2KA(pa) ((void *)((size_t)(pa) + KERNEL_VA_OFFSET))

/**
 * @brief 内核物理内存虚拟地址偏移
 * 
 * 40'0000'0000 => 5F'FFFF'FFFF (128GB)
 *
 * @note 与KERNEL_VA_OFFSET不同之处在于, 该偏移用于内核访问的物理地址.
 *
 */
#define KPHY_VA_OFFSET (size_t)(0xFFFF'FFC0'0000'0000ULL)

// 在post init阶段后才需要区分物理地址和内核使用的物理地址
#define KPA2PA(ka) \
    (post_init_flag ? ((void *)((size_t)(ka) - KPHY_VA_OFFSET)) : (ka))

#define PA2KPA(pa) \
    (post_init_flag ? ((void *)((size_t)(pa) + KPHY_VA_OFFSET)) : (pa))
