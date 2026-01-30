/**
 * @file gfp.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页框分配器接口
 * @version alpha-1.0.0
 * @date 2026-01-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/trait.h>
#include <sus/types.h>

#include <cstddef>

template <typename T>
concept PageFrameAllocatorTrait = requires(
    MemRegion *regions, size_t region_count, void *ptr, size_t frame_count) {
    {
        T::pre_init(regions, region_count)
    } -> std::same_as<void>;
    {
        T::post_init()
    } -> std::same_as<void>;
    {
        T::alloc_frame()
    } -> std::same_as<void *>;
    {
        T::alloc_frame(frame_count)
    } -> std::same_as<void *>;
    {
        T::free_frame(ptr)
    } -> std::same_as<void>;
    {
        T::free_frame(ptr, frame_count)
    } -> std::same_as<void>;
};

/**
 * @brief 线性增长GFP
 *
 */
class LinearGrowGFP {
    static void *baseaddr;
    static void *curaddr;
    static void *boundary;

public:
    static void pre_init(MemRegion *regions, size_t region_count);
    static void post_init();
    static void *alloc_frame(size_t frame_count = 1);
    static void free_frame(void *ptr, size_t frame_count = 1);
};

static_assert(PageFrameAllocatorTrait<LinearGrowGFP>,
              "LinearGrowthGFP 不满足 PageFrameAllocatorTrait");

constexpr size_t PAGESIZE = 0x1000;  // 4KB

// 向上对齐到页边界
constexpr umb_t page_align_up(umb_t addr_val) {
    return (addr_val + 0xFFF) & ~0xFFF;
}

// 向下对齐到页边界
constexpr umb_t page_align_down(umb_t addr_val) {
    return addr_val & ~0xFFF;
}