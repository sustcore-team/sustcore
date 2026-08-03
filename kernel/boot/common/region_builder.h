/**
 * @file region_builder.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动物理内存区域归一化工具
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/boot.h>

// 无堆、无静态状态且强制内联，供两个启动协议在永久页表前复用。
namespace boot::region {
    __ATTR_ALWAYS_INLINE__ bool empty(const MemoryRegion &region) noexcept {
        return region.area.nullable();
    }

    __ATTR_ALWAYS_INLINE__ addr_t end(const MemoryRegion &region) noexcept {
        return region.area.end.arith();
    }

    __ATTR_ALWAYS_INLINE__ bool less(const MemoryRegion &lhs, const MemoryRegion &rhs) noexcept {
        if (lhs.area.begin != rhs.area.begin)
            return lhs.area.begin < rhs.area.begin;
        if (lhs.area.end != rhs.area.end)
            return lhs.area.end < rhs.area.end;
        return static_cast<u32_t>(lhs.type) < static_cast<u32_t>(rhs.type);
    }

    __ATTR_ALWAYS_INLINE__ size_t normalize(MemoryRegion *regions, size_t cnt) noexcept {
        for (size_t idx = 1; idx < cnt; ++idx) {
            MemoryRegion key = regions[idx];
            size_t cursor    = idx;
            while (cursor > 0 && less(key, regions[cursor - 1])) {
                regions[cursor] = regions[cursor - 1];
                --cursor;
            }
            regions[cursor] = key;
        }
        size_t dst = 0;
        for (size_t idx = 0; idx < cnt; ++idx) {
            if (empty(regions[idx]))
                continue;
            if (dst > 0 && regions[dst - 1].type == regions[idx].type &&
                end(regions[dst - 1]) >= regions[idx].area.begin.arith())
            {
                const addr_t region_end = end(regions[idx]);
                if (end(regions[dst - 1]) < region_end) {
                    regions[dst - 1].area.end = PhyAddr(region_end);
                }
                continue;
            }
            regions[dst++] = regions[idx];
        }
        return dst;
    }
}  // namespace boot::region
