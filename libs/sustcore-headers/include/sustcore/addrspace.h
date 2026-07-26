/**
 * @file addrspace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 地址空间
 * @version 0.1.0-dev.1
 * @date 2026-07-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

// Virtual Address Area:         [NULL_ADDR, MAX_ADDR]
// Physical Address Area:        [NULL_ADDR, KPA_START)
// Kernel Virtual Address Area:  [KVA_START, MAX_ADDR]
// Kernel Physical Address Area: [KPA_START, KVA_START)
#define NULL_ADDR (0x0000'0000'0000'0000ULL)
#define MAX_ADDR  (0xFFFF'FFFF'FFFF'FFFFULL)
#define KVA_START (0xFFFF'FFFF'0000'0000ULL)
#define KPA_START (0xFFFF'FFC0'0000'0000ULL)

#define PAGE_SIZE (4096)