/**
 * @file kaddr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核地址空间
 * @version alpha-1.0.0
 * @date 2026-02-01
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/addr.h>
#include <arch/description.h>

namespace ker_paddr {
    void init(void *upper_bound);
    void mapping_kernel_areas(PageMan &man);
}  // namespace ker_paddr