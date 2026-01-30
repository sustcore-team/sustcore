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

#include <configuration.h>

// 全局new/delete操作符重载，使用线性增长分配器
// 但绝大多数情况下, 你都应该使用slab分配器或其他更高级的分配器
// 这要求你为你的类实现自定义的new/delete操作符

void* operator new(size_t size) {
    return Allocator::malloc(size);
}

void operator delete(void* ptr) noexcept {
    Allocator::free(ptr);
}

void* operator new[](size_t size) {
    return Allocator::malloc(size);
}

void operator delete[](void* ptr) noexcept {
    Allocator::free(ptr);
}

// Placement new/delete
void* operator new(size_t size, void* ptr) noexcept {
    return ptr;
}

void operator delete(void* ptr, void*) noexcept {
    Allocator::free(ptr);
}

void* operator new[](size_t size, void* ptr) noexcept {
    return ptr;
}

void operator delete[](void* ptr, void*) noexcept {
    Allocator::free(ptr);
}

// C++ 运行时支持 (C++ Runtime Support)
// 这段代码定义了链接器缺失的符号，满足 C++ 静态对象管理的 ABI 需求
extern "C" {
void *__dso_handle = 0;

// 内核不会退出，所以我们不需要真正的“退出时析构”逻辑，直接返回 0 即可
int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}
}