/**
 * @file ctxlayout.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Hart 与 LoongArch 上下文布局常量
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define CTX_SLOT_SIZE         8
#define CTX_SLOT_SHIFT        3
#define CTX_SLOT_OFFSET(slot) ((slot) << CTX_SLOT_SHIFT)

#define HART_CONTEXT_SIZE 512
#define MAX_HARTS         32

#define CTX_RA_SLOT     0
#define CTX_TP_SLOT     1
#define CTX_SP_SLOT     2
#define CTX_A0_SLOT     3
#define CTX_A1_SLOT     4
#define CTX_A2_SLOT     5
#define CTX_A3_SLOT     6
#define CTX_A4_SLOT     7
#define CTX_A5_SLOT     8
#define CTX_A6_SLOT     9
#define CTX_A7_SLOT     10
#define CTX_T0_SLOT     11
#define CTX_T1_SLOT     12
#define CTX_T2_SLOT     13
#define CTX_T3_SLOT     14
#define CTX_T4_SLOT     15
#define CTX_T5_SLOT     16
#define CTX_T6_SLOT     17
#define CTX_T7_SLOT     18
#define CTX_T8_SLOT     19
#define CTX_U0_SLOT     20
#define CTX_FP_SLOT     21
#define CTX_S0_SLOT     22
#define CTX_S1_SLOT     23
#define CTX_S2_SLOT     24
#define CTX_S3_SLOT     25
#define CTX_S4_SLOT     26
#define CTX_S5_SLOT     27
#define CTX_S6_SLOT     28
#define CTX_S7_SLOT     29
#define CTX_S8_SLOT     30
#define CTX_ERA_SLOT    31
#define CTX_ESTAT_SLOT  32
#define CTX_KSTACK_SLOT 33
#define CTX_SLOT_COUNT  34
