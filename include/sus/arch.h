/**
 * @file arch.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构相关函数声明
 *        这部分包含与特定架构相关的初始化和功能实现
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stddef.h>

/**
 * @brief 架构特定初始化
 * 
 */
void arch_init(void);

/**
 * @brief 架构特定收尾操作
 * 
 */
void arch_terminate(void);

enum {
    MEM_REGION_FREE = 0, // 可用内存
    MEM_REGION_RESERVED = 1, // 保留内存
    MEM_REGION_ACPI_RECLAIMABLE = 2, // ACPI可回收内存
    MEM_REGION_ACPI_NVS = 3, // ACPI NVS内存
    MEM_REGION_BAD_MEMORY = 4 // 坏内存
};

/**
 * @brief 内存区域结构体
 * 
 */
typedef struct __MemRegion__ {
    void *addr; // 区域起始地址
    size_t size; // 区域大小
    int status; // 区域状态
    struct __MemRegion__ *next; // 指向下一个内存区域
} MemRegion;

/**
 * @brief 获取内存布局
 * 
 * 要求kmalloc已被初始化
 * 
 * @return MemRegion* 内存区域链表头指针
 */
MemRegion *arch_get_memory_layout(void);
