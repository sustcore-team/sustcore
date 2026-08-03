/**
 * @file addrspace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Sustcore 内核地址空间标识和相关接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

// Physical/Virtual Address Area: [NULL_ADDR, KPA_START)
// High Virtual Address Area:     [KPA_START, MAX_ADDR]
// Kernel Virtual Address Area:  [KVA_START, MAX_ADDR]
// Kernel Physical Address Area: [KPA_START, KVA_START)
#define NULL_ADDR (0x0000'0000'0000'0000ULL)
#define MAX_ADDR  (0xFFFF'FFFF'FFFF'FFFFULL)
#define KVA_START (0xFFFF'FFFF'0000'0000ULL)
#define KPA_START (0xFFFF'FFC0'0000'0000ULL)

#define PAGE_SIZE (4096)
