/**
 * @file alloc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 分配器
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <mem/gfp.h>

#include <concepts>
#include <cstddef>

template <typename T>
concept AllocatorTrait = requires(size_t size, void *ptr) {
    {
        T::malloc(size)
    } -> std::same_as<void *>;
    {
        T::free(ptr)
    } -> std::same_as<void>;
    {
        T::init()
    } -> std::same_as<void>;
};

template <typename T, typename ObjType>
concept KOPTrait = requires(T *kop, ObjType *obj) {
    {
        kop->alloc()
    } -> std::same_as<ObjType *>;
    {
        kop->free(obj)
    } -> std::same_as<void>;
    {
        T::instance()
    } -> std::same_as<T &>;
};

class LinearGrowAllocator {
private:
    static constexpr size_t SIZE = 0x10000;  // 64KB
    static char LGA_HEAP[LinearGrowAllocator::SIZE];
    static size_t lga_offset;

public:
    /**
     * @brief 分配内存
     *
     * @param size 要分配的大小
     * @return void* 分配到的内存地址
     */
    static void *malloc(size_t size);
    /**
     * @brief 释放内存
     *
     * @param ptr 要释放的内存地址
     */
    static void free(void *ptr);
    /**
     * @brief 初始化函数
     *
     */
    static void init(void);
};

static_assert(AllocatorTrait<LinearGrowAllocator>,
              "LinearGrowAllocator 不满足 AllocatorTrait");

template <typename T, AllocatorTrait Allocator>

class SimpleKOP {
protected:
    static SimpleKOP _INSTANCE;

public:
    SimpleKOP()  = default;
    ~SimpleKOP() = default;
    static SimpleKOP &instance() {
        return _INSTANCE;
    }
    T *alloc() {
        return (T *)Allocator::malloc(sizeof(T));
    }
    void free(T *obj) {
        Allocator::free((void *)obj);
    }
};

template <typename T, AllocatorTrait Allocator>
SimpleKOP<T, Allocator> SimpleKOP<T, Allocator>::_INSTANCE;

static_assert(KOPTrait<SimpleKOP<int, LinearGrowAllocator>, int>,
              "SimpleKOP 不满足 KOPTrait");