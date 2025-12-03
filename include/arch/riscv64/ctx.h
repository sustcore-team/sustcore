/**
 * @file ctx.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <arch/riscv64/csr.h>
#include <sus/bits.h>

/**
 * @brief 保存的寄存器列表
 *
 */
typedef struct {
    umb_t regs[31];         // x1 - x31
    umb_t sepc;             // 保存的sepc
    csr_sstatus_t sstatus;  // 保存的sstatus
} InterruptContextRegisterList;

typedef InterruptContextRegisterList RegCtx;

#define CTX_PC(ctx) ((ctx)->sepc)
#define CTX_SP(ctx) ((ctx)->regs[1])