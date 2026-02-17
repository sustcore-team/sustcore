/**
 * @file gmm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通用内存管理器
 * @version alpha-1.0.0
 * @date 2026-01-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <mem/addr.h>

class GMM {
public:
    static void init(void);
    static PhyAddr get_page(int cnt = 1);
    static void put_page(PhyAddr paddr, int cnt = 1);
    static PhyAddr clone_page(PhyAddr paddr, int cnt = 1);
};