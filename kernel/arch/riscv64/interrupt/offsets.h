/**
 * @file offsets.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 陷阱帧汇编偏移
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define RV_TF_RA      0
#define RV_TF_SP      8
#define RV_TF_GP      16
#define RV_TF_TP      24
#define RV_TF_T0      32
#define RV_TF_T1      40
#define RV_TF_T2      48
#define RV_TF_S0      56
#define RV_TF_S1      64
#define RV_TF_A0      72
#define RV_TF_A1      80
#define RV_TF_A2      88
#define RV_TF_A3      96
#define RV_TF_A4      104
#define RV_TF_A5      112
#define RV_TF_A6      120
#define RV_TF_A7      128
#define RV_TF_S2      136
#define RV_TF_S3      144
#define RV_TF_S4      152
#define RV_TF_S5      160
#define RV_TF_S6      168
#define RV_TF_S7      176
#define RV_TF_S8      184
#define RV_TF_S9      192
#define RV_TF_S10     200
#define RV_TF_S11     208
#define RV_TF_T3      216
#define RV_TF_T4      224
#define RV_TF_T5      232
#define RV_TF_T6      240
#define RV_TF_SEPC    248
#define RV_TF_SSTATUS 256
#define RV_TF_STVAL   264
#define RV_TF_SCAUSE  272
#define RV_TF_SIZE    288
