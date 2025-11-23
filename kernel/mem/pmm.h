/**
 * @file pmm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理内存管理器
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
void pmm_init(MemRegion *layout);

/**
 * @brief 物理内存管理器后期初始化
 *
 * 在post_init阶段调用.
 * 将PMM的数据结构迁移到高地址处.
 *
 */
void pmm_post_init(void);

/**
 * @brief 分配一个物理页(返回物理页地址)
 *
 * @return void* 分配到的物理页地址
 */
void *alloc_page(void);

/**
 * @brief 分配多个物理页(返回物理页地址)
 *
 * @param pagecnt 要分配物理页数
 * @return void* 分配到的物理页起始地址
 */
void *alloc_pages(int pagecnt);

/**
 * @brief 分配多个物理页(返回物理页地址)
 *
 * @param order 2^order位要分配物理页数
 * @return void* 分配到的物理页起始地址
 */
void *alloc_pages_in_order(int order);

/**
 * @brief 释放一个物理页
 *
 * @param paddr 物理页地址
 */
void free_page(void *paddr);

/**
 * @brief 释放多个物理页
 *
 * @param paddr 物理页地址
 * @param pagecnt 物理页数
 */
void free_pages(void *paddr, int pagecnt);

/**
 * @brief 释放多个物理页
 *
 * @param paddr 物理页地址
 * @param order 2^order个物理页
 */
void free_pages_in_order(void *paddr, int order);