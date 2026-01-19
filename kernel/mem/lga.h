/**
 * @file lga.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 线性增长分配器
 * @version alpha-1.0.0
 * @date 2026-01-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/alloc.h>

class LinearGrowAllocator {
private:
    static constexpr size_t SIZE = 0x10000; // 64KB
    static char LGA_HEAP[LinearGrowAllocator::SIZE];
    static size_t lga_offset;
public:
    /**
     * @brief 分配内存
     *
     * @param size 要分配的大小
     * @return void* 分配到的内存地址
     */
    static void* malloc(size_t size);
    /**
     * @brief 释放内存
     *
     * @param ptr 要释放的内存地址
     */
    static void free(void* ptr);
};

static_assert(AllocatorTrait<LinearGrowAllocator>,
              "LinearGrowAllocator 不满足 AllocatorTrait");