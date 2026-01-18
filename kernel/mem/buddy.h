/**
 * @file buddy.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Buddy页框分配器
 * @version alpha-1.0.0
 * @date 2025-11-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <stddef.h>
#include <sus/arch.h>
#include <sus/attributes.h>
#include <sus/bits.h>
#include <sus/paging.h>

/**
 * @brief 初始化物理内存管理器
 *
 * @param layout 物理内存布局
 *
 */
void buddy_pre_init(MemRegion *layout);

/**
 * @brief 物理内存管理器后期初始化
 *
 * 在post_init阶段调用.
 * 将Buddy的数据结构迁移到高地址处.
 *
 */
void buddy_post_init(void);

/**
 * @brief 分配一个物理页(返回物理页地址)
 *
 * @return void* 分配到的物理页地址
 */
void *buddy_alloc_frame(void);

/**
 * @brief 分配多个物理页(返回物理页地址)
 *
 * @param count 要分配物理页数
 * @return void* 分配到的物理页起始地址
 */
void *buddy_alloc_frames(size_t count);

/**
 * @brief 分配多个物理页(返回物理页地址)
 *
 * @param order 2^order位要分配物理页数
 * @return void* 分配到的物理页起始地址
 */
void *buddy_alloc_frames_in_order(int order);

/**
 * @brief 释放一个物理页
 *
 * @param frame 物理页地址
 */
void buddy_free_frame(void *frame);

/**
 * @brief 释放多个物理页
 *
 * @param frame 物理页地址
 * @param count 物理页数
 */
void buddy_free_frames(void *frame, size_t count);

/**
 * @brief 释放多个物理页
 *
 * @param frame 物理页地址
 * @param order 2^order个物理页
 */
void buddy_free_frames_in_order(void *frame, int order);

/**
 * @brief 计算需要的阶数以容纳指定页数
 *
 * @param count 页数
 * @return int 阶数
 */
int pages2order(size_t count);