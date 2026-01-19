/**
 * @file cxa.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief C++ ABI
 * @version alpha-1.0.0
 * @date 2026-01-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/alloc.h>
#include <mem/lga.h>

// 全局new/delete操作符重载，使用线性增长分配器
// 但绝大多数情况下, 你都应该使用slab分配器或其他更高级的分配器
// 这要求你为你的类实现自定义的new/delete操作符

void* operator new(size_t size) {
    return LinearGrowAllocator::malloc(size);
}

void operator delete(void* ptr) noexcept {
    LinearGrowAllocator::free(ptr);
}

void* operator new[](size_t size) {
    return LinearGrowAllocator::malloc(size);
}

void operator delete[](void* ptr) noexcept {
    LinearGrowAllocator::free(ptr);
}

// Placement new/delete
void* operator new(size_t size, void* ptr) noexcept {
    return ptr;
}

void operator delete(void* ptr, void*) noexcept {
    LinearGrowAllocator::free(ptr);
}

void* operator new[](size_t size, void* ptr) noexcept {
    return ptr;
}

void operator delete[](void* ptr, void*) noexcept {
    LinearGrowAllocator::free(ptr);
}