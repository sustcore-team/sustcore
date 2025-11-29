/**
 * @file alloc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存分配器
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <kmod/alloc.h>

/**
 * @brief 初始化内存分配器
 *
 * @param heap_ptr 堆指针
 */
void init_malloc(void *heap_ptr);