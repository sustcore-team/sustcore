/**
 * @file alloc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 分配器接口
 * @version alpha-1.0.0
 * @date 2025-11-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <stddef.h>

/**
 * @brief Kernel内存分配器
 *
 * @param size 分配大小
 * @return void* 分配到的内存指针
 */
void *kmalloc(size_t size);

/**
 * @brief Kernel内存释放器
 *
 * @param ptr 需要释放的内存指针
 */
void kfree(void *ptr);

/**
 * @brief 初始化分配器
 *
 */
void init_allocator(void);

/**
 * @brief 初始化分配器第二阶段
 *
 * 应当由pmm_init在物理内存管理器初始化完成后调用
 *
 */
void init_allocator_stage2(void);

/**
 * @brief 初始化分配器第三阶段
 *
 * 应当由post_init在内核页表被设置完成后调用
 *
 */
void init_allocator_stage3(void);