/**
 * @file frame.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 陷阱帧布局与汇编 ABI
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/interrupt/offsets.h>
#include <arch/namespace.h>
#include <tay/bits.h>

#include <cstddef>

namespace loongarch64::hal {
    struct alignas(16) TrapFrame {
        xlen_t ra, tp, sp;
        xlen_t a0, a1, a2, a3, a4, a5, a6, a7;
        xlen_t t0, t1, t2, t3, t4, t5, t6, t7, t8;
        xlen_t u0, fp, s0, s1, s2, s3, s4, s5, s6, s7, s8;
        xlen_t era, prmd, badv, estat;
    };

#define LA_ASSERT_TRAP_FIELD(member, reg) static_assert(offsetof(TrapFrame, member) == LA_TF_##reg)
    LA_ASSERT_TRAP_FIELD(ra, RA);
    LA_ASSERT_TRAP_FIELD(tp, TP);
    LA_ASSERT_TRAP_FIELD(sp, SP);
    LA_ASSERT_TRAP_FIELD(a0, A0);
    LA_ASSERT_TRAP_FIELD(a1, A1);
    LA_ASSERT_TRAP_FIELD(a2, A2);
    LA_ASSERT_TRAP_FIELD(a3, A3);
    LA_ASSERT_TRAP_FIELD(a4, A4);
    LA_ASSERT_TRAP_FIELD(a5, A5);
    LA_ASSERT_TRAP_FIELD(a6, A6);
    LA_ASSERT_TRAP_FIELD(a7, A7);
    LA_ASSERT_TRAP_FIELD(t0, T0);
    LA_ASSERT_TRAP_FIELD(t1, T1);
    LA_ASSERT_TRAP_FIELD(t2, T2);
    LA_ASSERT_TRAP_FIELD(t3, T3);
    LA_ASSERT_TRAP_FIELD(t4, T4);
    LA_ASSERT_TRAP_FIELD(t5, T5);
    LA_ASSERT_TRAP_FIELD(t6, T6);
    LA_ASSERT_TRAP_FIELD(t7, T7);
    LA_ASSERT_TRAP_FIELD(t8, T8);
    LA_ASSERT_TRAP_FIELD(u0, U0);
    LA_ASSERT_TRAP_FIELD(fp, FP);
    LA_ASSERT_TRAP_FIELD(s0, S0);
    LA_ASSERT_TRAP_FIELD(s1, S1);
    LA_ASSERT_TRAP_FIELD(s2, S2);
    LA_ASSERT_TRAP_FIELD(s3, S3);
    LA_ASSERT_TRAP_FIELD(s4, S4);
    LA_ASSERT_TRAP_FIELD(s5, S5);
    LA_ASSERT_TRAP_FIELD(s6, S6);
    LA_ASSERT_TRAP_FIELD(s7, S7);
    LA_ASSERT_TRAP_FIELD(s8, S8);
    LA_ASSERT_TRAP_FIELD(era, ERA);
    LA_ASSERT_TRAP_FIELD(prmd, PRMD);
    LA_ASSERT_TRAP_FIELD(badv, BADV);
    LA_ASSERT_TRAP_FIELD(estat, ESTAT);
    static_assert(sizeof(TrapFrame) == LA_TF_SIZE);
    static_assert(alignof(TrapFrame) == 16);
#undef LA_ASSERT_TRAP_FIELD
}  // namespace loongarch64::hal
