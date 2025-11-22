/**
 * @file universal.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISV64通用分页部分
 * @version alpha-1.0.0
 * @date 2025-11-22
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

enum {
    RWX_MODE_P   = 0b000,  // 指向下一级页表
    RWX_MODE_R   = 0b001,  // 读
    RWX_MODE_RW  = 0b011,  // 读写
    RWX_MODE_X   = 0b100,  // 执行
    RWX_MODE_RX  = 0b101,  // 读执行
    RWX_MODE_RWX = 0b111,  // 读写执行
    RWX_MASK_R   = 0b001,  // 读掩码
    RWX_MASK_W   = 0b010,  // 写掩码
    RWX_MASK_X   = 0b100,  // 执行掩码
};

#define flush_tlb() asm volatile("sfence.vma")