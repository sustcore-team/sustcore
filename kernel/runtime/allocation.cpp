/**
 * @file allocation.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 语言运行时内存分配 ABI 适配
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

// C/C++ 分配 ABI 适配层：在 heap_ready 前所有可失败分配返回 nullptr。
#include <log.h>
#include <memory/slab/heap.h>
#include <tay/allocator.h>
#include <tay/bits.h>

#include <cstddef>
#include <new>

namespace kernel::runtime::detail {
    [[nodiscard]] void *try_heap_allocate(size_t sz, size_t alignment) noexcept {
        if (!memory::heap_ready()) {
            return nullptr;
        }
        const auto result = memory::alloc(sz, alignment);
        return result ? *result : nullptr;
    }

    void try_free(void *ptr) noexcept {
        if (ptr != nullptr) {
            memory::dealloc(ptr);
        }
    }

    [[nodiscard]] void *required_heap_allocate(size_t sz, size_t alignment) {
        void *const ptr = try_heap_allocate(sz, alignment);
        if (ptr == nullptr) {
            kernel::log::panic("内核堆已耗尽");
        }
        return ptr;
    }
}  // namespace kernel::runtime::detail

void *operator new(size_t sz) {
    return kernel::runtime::detail::required_heap_allocate(sz, alignof(std::max_align_t));
}

void *operator new[](size_t sz) {
    return kernel::runtime::detail::required_heap_allocate(sz, alignof(std::max_align_t));
}

void *operator new(size_t sz, std::align_val_t alignment) {
    return kernel::runtime::detail::required_heap_allocate(sz, static_cast<size_t>(alignment));
}

void *operator new[](size_t sz, std::align_val_t alignment) {
    return kernel::runtime::detail::required_heap_allocate(sz, static_cast<size_t>(alignment));
}

void *operator new(size_t sz, const std::nothrow_t &) noexcept {
    return kernel::runtime::detail::try_heap_allocate(sz, alignof(std::max_align_t));
}

void *operator new[](size_t sz, const std::nothrow_t &) noexcept {
    return kernel::runtime::detail::try_heap_allocate(sz, alignof(std::max_align_t));
}

void *operator new(size_t sz, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return kernel::runtime::detail::try_heap_allocate(sz, static_cast<size_t>(alignment));
}

void *operator new[](size_t sz, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return kernel::runtime::detail::try_heap_allocate(sz, static_cast<size_t>(alignment));
}

void operator delete(void *ptr) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete[](void *ptr) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete(void *ptr, size_t) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete[](void *ptr, size_t) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete(void *ptr, std::align_val_t) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete[](void *ptr, std::align_val_t) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete(void *ptr, size_t, std::align_val_t) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete[](void *ptr, size_t, std::align_val_t) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete(void *ptr, const std::nothrow_t &) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete[](void *ptr, const std::nothrow_t &) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete(void *ptr, std::align_val_t, const std::nothrow_t &) noexcept {
    kernel::runtime::detail::try_free(ptr);
}
void operator delete[](void *ptr, std::align_val_t, const std::nothrow_t &) noexcept {
    kernel::runtime::detail::try_free(ptr);
}

namespace tay {
    void *__alloc(size_t sz, size_t alignment) noexcept {
        return kernel::runtime::detail::try_heap_allocate(sz, alignment);
    }

    void __free(void *ptr, size_t, size_t) noexcept {
        kernel::runtime::detail::try_free(ptr);
    }
}  // namespace tay

extern "C" void *malloc(size_t sz) {
    return kernel::runtime::detail::try_heap_allocate(sz, alignof(std::max_align_t));
}

extern "C" void free(void *ptr) {
    kernel::runtime::detail::try_free(ptr);
}

extern "C" void *calloc(size_t cnt, size_t sz) {
    if (sz != 0 && cnt > static_cast<size_t>(-1) / sz) {
        return nullptr;
    }
    const size_t bytes = cnt * sz;
    auto *const ptr    = static_cast<u8_t *>(malloc(bytes));
    if (ptr == nullptr) {
        return nullptr;
    }
    for (size_t idx = 0; idx < bytes; ++idx) {
        ptr[idx] = 0;
    }
    return ptr;
}

extern "C" void *realloc(void *ptr, size_t sz) {
    if (!memory::heap_ready()) {
        return nullptr;
    }
    return memory::heap_allocator().reallocate(ptr, sz);
}
