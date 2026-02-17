/**
 * @file gfp.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页框分配器实现
 * @version alpha-1.0.0
 * @date 2026-01-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <mem/addr.h>
#include <mem/gfp.h>
#include <sus/types.h>

PhyAddr LinearGrowGFP::baseaddr = PhyAddr::null;
PhyAddr LinearGrowGFP::curaddr = PhyAddr::null;
PhyAddr LinearGrowGFP::boundary = PhyAddr::null;

void LinearGrowGFP::pre_init(MemRegion *regions, size_t region_count) {
    PhyAddr _baseaddr = PhyAddr::null;
    // 从regions中找到大小最大的可用内存区域，作为线性增长GFP的内存池
    size_t max_size   = 0;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].status == MemRegion::MemoryStatus::FREE) {
            if (regions[i].size > max_size) {
                max_size  = regions[i].size;
                _baseaddr = regions[i].ptr;
            }
        }
    }

    // 将 baseaddr 向上对齐, boundary 向下对齐
    baseaddr          = _baseaddr.page_align_up();
    curaddr           = baseaddr;
    PhyAddr _boundary = baseaddr + max_size;
    boundary          = _boundary.page_align_down();
}

void LinearGrowGFP::post_init(MemRegion *regions, size_t region_count) {
    // 线性增长GFP不需要在post_init阶段执行任何操作
}