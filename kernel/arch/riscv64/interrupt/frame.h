/**
 * @file frame.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 陷阱帧布局与汇编 ABI
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>
#include <arch/riscv64/interrupt/offsets.h>
#include <tay/bits.h>

#include <cstddef>

namespace riscv64::hal {
    struct alignas(16) TrapFrame {
        xlen_t ra, sp, gp, tp;
        xlen_t t0, t1, t2, s0, s1;
        xlen_t a0, a1, a2, a3, a4, a5, a6, a7;
        xlen_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
        xlen_t t3, t4, t5, t6;
        xlen_t sepc, sstatus, stval, scause;
    };

#define RV_ASSERT_TRAP_FIELD(member, reg) static_assert(offsetof(TrapFrame, member) == RV_TF_##reg)
    RV_ASSERT_TRAP_FIELD(ra, RA);
    RV_ASSERT_TRAP_FIELD(sp, SP);
    RV_ASSERT_TRAP_FIELD(gp, GP);
    RV_ASSERT_TRAP_FIELD(tp, TP);
    RV_ASSERT_TRAP_FIELD(t0, T0);
    RV_ASSERT_TRAP_FIELD(t1, T1);
    RV_ASSERT_TRAP_FIELD(t2, T2);
    RV_ASSERT_TRAP_FIELD(s0, S0);
    RV_ASSERT_TRAP_FIELD(s1, S1);
    RV_ASSERT_TRAP_FIELD(a0, A0);
    RV_ASSERT_TRAP_FIELD(a1, A1);
    RV_ASSERT_TRAP_FIELD(a2, A2);
    RV_ASSERT_TRAP_FIELD(a3, A3);
    RV_ASSERT_TRAP_FIELD(a4, A4);
    RV_ASSERT_TRAP_FIELD(a5, A5);
    RV_ASSERT_TRAP_FIELD(a6, A6);
    RV_ASSERT_TRAP_FIELD(a7, A7);
    RV_ASSERT_TRAP_FIELD(s2, S2);
    RV_ASSERT_TRAP_FIELD(s3, S3);
    RV_ASSERT_TRAP_FIELD(s4, S4);
    RV_ASSERT_TRAP_FIELD(s5, S5);
    RV_ASSERT_TRAP_FIELD(s6, S6);
    RV_ASSERT_TRAP_FIELD(s7, S7);
    RV_ASSERT_TRAP_FIELD(s8, S8);
    RV_ASSERT_TRAP_FIELD(s9, S9);
    RV_ASSERT_TRAP_FIELD(s10, S10);
    RV_ASSERT_TRAP_FIELD(s11, S11);
    RV_ASSERT_TRAP_FIELD(t3, T3);
    RV_ASSERT_TRAP_FIELD(t4, T4);
    RV_ASSERT_TRAP_FIELD(t5, T5);
    RV_ASSERT_TRAP_FIELD(t6, T6);
    RV_ASSERT_TRAP_FIELD(sepc, SEPC);
    RV_ASSERT_TRAP_FIELD(sstatus, SSTATUS);
    RV_ASSERT_TRAP_FIELD(stval, STVAL);
    RV_ASSERT_TRAP_FIELD(scause, SCAUSE);
    static_assert(sizeof(TrapFrame) == RV_TF_SIZE);
    static_assert(alignof(TrapFrame) == 16);
#undef RV_ASSERT_TRAP_FIELD
}  // namespace riscv64::hal
