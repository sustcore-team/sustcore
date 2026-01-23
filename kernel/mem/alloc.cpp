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

#include <mem/alloc.h>
#include <basecpp/logger.h>
#include "kio.h"

char LinearGrowAllocator::LGA_HEAP[LinearGrowAllocator::SIZE];
size_t LinearGrowAllocator::lga_offset = 0;

DECLARE_LOGGER(KernelIO, LogLevel::INFO, Lga)

void* LinearGrowAllocator::malloc(size_t size) {
    if (lga_offset + size > SIZE) {
        logger.fatal("%s", "内存不足");
        return nullptr;  // 内存不足
    }
    void* ptr   = &LGA_HEAP[lga_offset];
    lga_offset += size;
    return ptr;
}

void LinearGrowAllocator::free(void* ptr) {
    // 线性增长分配器不支持释放内存
    logger.fatal("%s", "线性增长分配器不支持释放内存");
    (void)ptr;  // 避免未使用参数警告
}