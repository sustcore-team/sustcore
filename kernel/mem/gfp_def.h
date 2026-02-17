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
#include <mem/addr.h>
#include <sus/types.h>

#include <cstddef>

template <typename T>
concept GFPTrait = requires(MemRegion *regions, size_t region_count, PhyAddr ptr,
                            size_t page_count) {
    {
        T::pre_init(regions, region_count)
    } -> std::same_as<void>;
    {
        T::post_init(regions, region_count)
    } -> std::same_as<void>;
    {
        T::get_free_page()
    } -> std::same_as<PhyAddr>;
    {
        T::get_free_page(page_count)
    } -> std::same_as<PhyAddr>;
    {
        T::template get_free_page<KernelStage::PRE_INIT>()
    } -> std::same_as<PhyAddr>;
    {
        T::template get_free_page<KernelStage::PRE_INIT>(page_count)
    } -> std::same_as<PhyAddr>;
    {
        T::template get_free_page<KernelStage::POST_INIT>()
    } -> std::same_as<PhyAddr>;
    {
        T::template get_free_page<KernelStage::POST_INIT>(page_count)
    } -> std::same_as<PhyAddr>;
    {
        T::put_page(ptr)
    } -> std::same_as<void>;
    {
        T::put_page(ptr, page_count)
    } -> std::same_as<void>;
    {
        T::template put_page<KernelStage::PRE_INIT>(ptr)
    } -> std::same_as<void>;
    {
        T::template put_page<KernelStage::PRE_INIT>(ptr, page_count)
    } -> std::same_as<void>;
    {
        T::template put_page<KernelStage::POST_INIT>(ptr)
    } -> std::same_as<void>;
    {
        T::template put_page<KernelStage::POST_INIT>(ptr, page_count)
    } -> std::same_as<void>;
};

/**
 * @brief 线性增长GFP
 *
 */
class LinearGrowGFP {
    static PhyAddr baseaddr;
    static PhyAddr curaddr;
    static PhyAddr boundary;
public:
    static void pre_init(MemRegion *regions, size_t region_count);
    static void post_init(MemRegion *regions, size_t region_count);
    template <KernelStage Stage = KernelStage::POST_INIT>
    static PhyAddr get_free_page(size_t page_count = 1) {
        PhyAddr _bound = curaddr + page_count * PAGESIZE;
        if (_bound > boundary) {
            return PhyAddr::null;  // 内存不足
        }
        PhyAddr ptr = curaddr;
        curaddr = _bound;
        return ptr;
    }
    template <KernelStage Stage = KernelStage::POST_INIT>
    static void put_page(PhyAddr addr, size_t page_count = 1) {
        // 线性增长GFP不支持释放页框，因此该函数不执行任何操作
    }
};

static_assert(
    GFPTrait<LinearGrowGFP>,
    "LinearGrowthGFP 不满足 GFPTrait");