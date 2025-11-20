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

#include <sus/bits.h>
#include <sus/attributes.h>
#include <stddef.h>

typedef void PhyscialMemoryLayout;

/**
 * @brief 初始化物理内存管理器
 * 
 * @param layout 物理内存布局
 * 
 */
void pmm_init(PhyscialMemoryLayout *layout);

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
 * @brief 将物理地址转换为内核虚拟地址
 * 
 * @param paddr 物理地址
 * @return void* 内核虚拟地址
 */
void *paddr2kaddr(void *paddr);