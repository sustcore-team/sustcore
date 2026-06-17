/**
 * @file paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页表相关宏
 * @version alpha-1.0.0
 * @date 2026-06-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define LA_PAGE_PRESENT  0x80
#define LA_PAGE_GLOBAL   0x40
#define LA_PAGE_CACHE_CC 0x10
#define LA_PAGE_MODIFIED 0x200
#define LA_PAGE_WRITE    0x100
#define LA_PAGE_DIRTY    0x2
#define LA_PAGE_VALID    0x1
#define LA_PTE_FLAGS     0x3d3
#define LA_PPN_MASK      0x0000FFFFFFFFF000ULL