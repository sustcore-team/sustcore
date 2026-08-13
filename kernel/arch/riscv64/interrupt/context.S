/**
 * @file context.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 线程调度上下文切换
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/context_offsets.h>
#include <arch/riscv64/interrupt/offsets.h>

    .section .text.scheduler.context, "ax", @progbits
    .align 4
    .global __switch_to
    .type __switch_to, @function
__switch_to:
    # a0 保存旧线程的被调用者保存寄存器，包含恢复该调用点所需的 ra/sp。
    sd ra, RV_CTX_RA(a0)
    sd sp, RV_CTX_SP(a0)
    sd s0, RV_CTX_S0(a0)
    sd s1, RV_CTX_S1(a0)
    sd s2, RV_CTX_S2(a0)
    sd s3, RV_CTX_S3(a0)
    sd s4, RV_CTX_S4(a0)
    sd s5, RV_CTX_S5(a0)
    sd s6, RV_CTX_S6(a0)
    sd s7, RV_CTX_S7(a0)
    sd s8, RV_CTX_S8(a0)
    sd s9, RV_CTX_S9(a0)
    sd s10, RV_CTX_S10(a0)
    sd s11, RV_CTX_S11(a0)

    # 从 a1 切换栈和返回地址；ret 将回到新线程上次停下的位置或 bootstrap。
    ld ra, RV_CTX_RA(a1)
    ld sp, RV_CTX_SP(a1)
    ld s0, RV_CTX_S0(a1)
    ld s1, RV_CTX_S1(a1)
    ld s2, RV_CTX_S2(a1)
    ld s3, RV_CTX_S3(a1)
    ld s4, RV_CTX_S4(a1)
    ld s5, RV_CTX_S5(a1)
    ld s6, RV_CTX_S6(a1)
    ld s7, RV_CTX_S7(a1)
    ld s8, RV_CTX_S8(a1)
    ld s9, RV_CTX_S9(a1)
    ld s10, RV_CTX_S10(a1)
    ld s11, RV_CTX_S11(a1)
    ret
    .size __switch_to, . - __switch_to

    .global _ZN7riscv643hal10enter_userERNS0_9TrapFrameEm
    .type _ZN7riscv643hal10enter_userERNS0_9TrapFrameEm, @function
_ZN7riscv643hal10enter_userERNS0_9TrapFrameEm:
    # a0 指向只使用一次的初始 TrapFrame；sscratch 保存该线程的内核栈顶。
    csrw sscratch, a1
    mv t6, a0
    ld t0, RV_TF_SEPC(t6)
    csrw sepc, t0
    ld t0, RV_TF_SSTATUS(t6)
    csrw sstatus, t0
    ld ra, RV_TF_RA(t6)
    ld sp, RV_TF_SP(t6)
    ld gp, RV_TF_GP(t6)
    ld tp, RV_TF_TP(t6)
    ld t0, RV_TF_T0(t6)
    ld t1, RV_TF_T1(t6)
    ld t2, RV_TF_T2(t6)
    ld s0, RV_TF_S0(t6)
    ld s1, RV_TF_S1(t6)
    ld a0, RV_TF_A0(t6)
    ld a1, RV_TF_A1(t6)
    ld a2, RV_TF_A2(t6)
    ld a3, RV_TF_A3(t6)
    ld a4, RV_TF_A4(t6)
    ld a5, RV_TF_A5(t6)
    ld a6, RV_TF_A6(t6)
    ld a7, RV_TF_A7(t6)
    ld s2, RV_TF_S2(t6)
    ld s3, RV_TF_S3(t6)
    ld s4, RV_TF_S4(t6)
    ld s5, RV_TF_S5(t6)
    ld s6, RV_TF_S6(t6)
    ld s7, RV_TF_S7(t6)
    ld s8, RV_TF_S8(t6)
    ld s9, RV_TF_S9(t6)
    ld s10, RV_TF_S10(t6)
    ld s11, RV_TF_S11(t6)
    ld t3, RV_TF_T3(t6)
    ld t4, RV_TF_T4(t6)
    ld t5, RV_TF_T5(t6)
    ld t6, RV_TF_T6(t6)
    sret
    .size _ZN7riscv643hal10enter_userERNS0_9TrapFrameEm, . - _ZN7riscv643hal10enter_userERNS0_9TrapFrameEm
