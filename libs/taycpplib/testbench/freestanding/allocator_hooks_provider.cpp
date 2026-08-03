/**
 * @file allocator_hooks_provider.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 freestanding 分配器钩子测试提供自定义分配实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/allocator.h>
#include <tay/bits.h>

#include <cstddef>

namespace {
    alignas(256) unsigned char storage[4096];
    size_t offset = 0;
}  // namespace

void* tay::__alloc(size_t size, size_t alignment) noexcept {
    const uintptr_t address = reinterpret_cast<uintptr_t>(storage + offset);
    const size_t padding    = static_cast<size_t>(-address) & (alignment - 1);
    if (size > sizeof(storage) - offset || padding > sizeof(storage) - offset - size) {
        return nullptr;
    }
    offset       += padding;
    void* result  = storage + offset;
    offset       += size;
    return result;
}

void tay::__free(void*, size_t, size_t) noexcept {}
