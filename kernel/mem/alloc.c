/**
 * @file alloc.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 分配器
 * @version alpha-1.0.0
 * @date 2025-11-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <mem/alloc.h>
#include <basec/logger.h>
#include <sus/bits.h>

typedef void *(*KAllocator)(size_t size);
typedef void (*KDeallocator)(void *ptr);

/**
 * @brief Kernel内存分配器
 * 
 */
static KAllocator __kmalloc__ = nullptr;

/**
 * @brief Kernel内存释放器
 * 
 */
static KDeallocator __kfree__ = nullptr;

/**
 * @brief 设置内存分配器/释放器
 * 
 * @param alloc 内存分配器
 * @param free_func 内存释放器
 */
static void set_dual_allocator(KAllocator allocator, KDeallocator deallocator) {
    __kmalloc__ = allocator;
    __kfree__ = deallocator;
}

void *kmalloc(size_t size) {
    if (__kmalloc__ != nullptr) {
        return __kmalloc__(size);
    }
    log_error("kmalloc: 内存分配器未设置");
    return nullptr;
}

void kfree(void *ptr) {
    if (__kfree__ != nullptr) {
        __kfree__(ptr);
        return;
    }
    log_error("kfree: 内存释放器未设置");
}

/**
 * @brief HEAP内存块
 * 
 */
__attribute__((section(".data.heap")))
static byte __HEAP__[16 * 1024 * 1024]; // 16MB初始化堆内存
static byte *__HEAP_TAIL__ = &__HEAP__[sizeof(__HEAP__) - 1]; // 堆内存尾
static byte *heap_ptr = nullptr; // 当前堆内存分配指针

void *__primitive_kmalloc__(size_t size) {
    if (heap_ptr + size > __HEAP_TAIL__) {
        log_error("__primitive_kmalloc__: 堆内存不足");
        return nullptr;
    }

    // 分配堆内存
    void *ptr = heap_ptr;
    heap_ptr += size;
    return ptr;
}

void __primitive_kfree__(void *ptr) {
    // 不上贤，使民不争；不贵难得之货，使民不为盗；不见可欲，使民心不乱。
    // 是以圣人之治，虚其心，实其腹；弱其志，强其骨。常使民无知无欲，使夫知者不敢为也。
    // 为无为，则无不治。
}

void init_allocator(void) {
    // 暂时使用data.heap区域的16MB堆内存
    heap_ptr = __HEAP__;
    set_dual_allocator(__primitive_kmalloc__, __primitive_kfree__);
}

void init_allocator_stage2(void) {
    // 输出stage1的堆使用情况
    log_info("__HEAP__: %p, size: %u bytes", __HEAP__, sizeof(__HEAP__));
    log_info("heap_ptr: %p, used: %u KB", heap_ptr, (heap_ptr - __HEAP__) / 1024);
}