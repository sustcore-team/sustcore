/**
 * @file lga.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 线性增长分配器
 * @version alpha-1.0.0
 * @date 2026-01-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <mem/lga.h>

char LinearGrowAllocator::LGA_HEAP[LinearGrowAllocator::SIZE];
size_t LinearGrowAllocator::lga_offset = 0;

void *LinearGrowAllocator::malloc(size_t size) {
    if (lga_offset + size > SIZE) {
        return nullptr; // 内存不足
    }
    void* ptr = &LGA_HEAP[lga_offset];
    lga_offset += size;
    return ptr;
}

void LinearGrowAllocator::free(void* ptr) {
    // 线性增长分配器不支持释放内存
    (void)ptr; // 避免未使用参数警告
}