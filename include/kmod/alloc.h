/**
 * @file alloc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 分配器接口
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <stddef.h>

/**
 * @brief 分配内存
 *
 * @param size 分配的内存大小
 * @return void* 指向分配内存的指针
 */
void *malloc(size_t size);

/**
 * @brief 释放内存
 *
 * @param ptr 指向要释放的内存的指针
 */
void free(void *ptr);