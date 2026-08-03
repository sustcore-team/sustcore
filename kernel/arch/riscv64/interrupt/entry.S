/**
 * @file entry.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 陷阱上下文保存与分发入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/interrupt/offsets.h>

    .section .text.trap.early, "ax", @progbits
    .align 4
    .global _riscv_early_trap_entry
_riscv_early_trap_entry:
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    tail arch_early_trap_handler

    .section .text.trap.runtime, "ax", @progbits
    .align 4
    .global _riscv_runtime_trap_entry
_riscv_runtime_trap_entry:
    # 先在当前内核栈构造 TrapFrame；原 sp 由分配后的新 sp 反推保存。
    addi sp, sp, -RV_TF_SIZE
    sd ra, RV_TF_RA(sp)
    sd t0, RV_TF_T0(sp)
    addi t0, sp, RV_TF_SIZE
    sd t0, RV_TF_SP(sp)
    sd gp, RV_TF_GP(sp)
    sd tp, RV_TF_TP(sp)
    sd t1, RV_TF_T1(sp)
    sd t2, RV_TF_T2(sp)
    sd s0, RV_TF_S0(sp)
    sd s1, RV_TF_S1(sp)
    sd a0, RV_TF_A0(sp)
    sd a1, RV_TF_A1(sp)
    sd a2, RV_TF_A2(sp)
    sd a3, RV_TF_A3(sp)
    sd a4, RV_TF_A4(sp)
    sd a5, RV_TF_A5(sp)
    sd a6, RV_TF_A6(sp)
    sd a7, RV_TF_A7(sp)
    sd s2, RV_TF_S2(sp)
    sd s3, RV_TF_S3(sp)
    sd s4, RV_TF_S4(sp)
    sd s5, RV_TF_S5(sp)
    sd s6, RV_TF_S6(sp)
    sd s7, RV_TF_S7(sp)
    sd s8, RV_TF_S8(sp)
    sd s9, RV_TF_S9(sp)
    sd s10, RV_TF_S10(sp)
    sd s11, RV_TF_S11(sp)
    sd t3, RV_TF_T3(sp)
    sd t4, RV_TF_T4(sp)
    sd t5, RV_TF_T5(sp)
    sd t6, RV_TF_T6(sp)
    csrr t0, sepc
    sd t0, RV_TF_SEPC(sp)
    csrr t0, sstatus
    sd t0, RV_TF_SSTATUS(sp)
    csrr t0, stval
    sd t0, RV_TF_STVAL(sp)
    csrr t0, scause
    sd t0, RV_TF_SCAUSE(sp)
    mv a0, sp
    call arch_runtime_trap_handler
    # C++ 可修改返回 PC 和状态，因此先回写 CSR，再恢复通用寄存器。
    ld t0, RV_TF_SEPC(sp)
    csrw sepc, t0
    ld t0, RV_TF_SSTATUS(sp)
    csrw sstatus, t0
    ld ra, RV_TF_RA(sp)
    ld gp, RV_TF_GP(sp)
    ld tp, RV_TF_TP(sp)
    ld t0, RV_TF_T0(sp)
    ld t1, RV_TF_T1(sp)
    ld t2, RV_TF_T2(sp)
    ld s0, RV_TF_S0(sp)
    ld s1, RV_TF_S1(sp)
    ld a0, RV_TF_A0(sp)
    ld a1, RV_TF_A1(sp)
    ld a2, RV_TF_A2(sp)
    ld a3, RV_TF_A3(sp)
    ld a4, RV_TF_A4(sp)
    ld a5, RV_TF_A5(sp)
    ld a6, RV_TF_A6(sp)
    ld a7, RV_TF_A7(sp)
    ld s2, RV_TF_S2(sp)
    ld s3, RV_TF_S3(sp)
    ld s4, RV_TF_S4(sp)
    ld s5, RV_TF_S5(sp)
    ld s6, RV_TF_S6(sp)
    ld s7, RV_TF_S7(sp)
    ld s8, RV_TF_S8(sp)
    ld s9, RV_TF_S9(sp)
    ld s10, RV_TF_S10(sp)
    ld s11, RV_TF_S11(sp)
    ld t3, RV_TF_T3(sp)
    ld t4, RV_TF_T4(sp)
    ld t5, RV_TF_T5(sp)
    ld t6, RV_TF_T6(sp)
    # 最后恢复被中断现场的 sp，TrapFrame 自此不可再访问。
    ld sp, RV_TF_SP(sp)
    sret
