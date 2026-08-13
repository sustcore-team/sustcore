/**
 * @file entry.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 陷阱上下文保存与分发入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/valdef.h>
#include <arch/loongarch64/interrupt/offsets.h>

    .section .text.trap.early, "ax", @progbits
    .align 12
    .global _loongarch_early_trap_entry
_loongarch_early_trap_entry:
    csrrd $a0, CSR_ESTAT
    csrrd $a1, CSR_ERA
    csrrd $a2, CSR_BADV
    b arch_early_trap_handler

    .section .text.trap.runtime, "ax", @progbits
    .align 12
    .global _loongarch_runtime_trap_entry
_loongarch_runtime_trap_entry:
    # 用户 trap 通过 SAVE0 切换到 Thread 内核栈；内核 trap 保持当前栈。
    csrwr $sp, CSR_SAVE0
    bnez $sp, .Lloongarch_user_trap
    csrwr $sp, CSR_SAVE0
    b .Lloongarch_stack_ready
.Lloongarch_user_trap:
    addi.d $sp, $sp, -LA_TF_SIZE
    st.d $r1, $sp, LA_TF_RA
    st.d $r2, $sp, LA_TF_TP
    st.d $r12, $sp, LA_TF_T0
    csrrd $r12, CSR_SAVE0
    st.d $r12, $sp, LA_TF_SP
    csrwr $zero, CSR_SAVE0
    b .Lloongarch_save_rest
.Lloongarch_stack_ready:
    addi.d $sp, $sp, -LA_TF_SIZE
    st.d $r1, $sp, LA_TF_RA
    st.d $r2, $sp, LA_TF_TP
    st.d $r12, $sp, LA_TF_T0
    addi.d $r12, $sp, LA_TF_SIZE
    st.d $r12, $sp, LA_TF_SP
.Lloongarch_save_rest:
    st.d $r4, $sp, LA_TF_A0
    st.d $r5, $sp, LA_TF_A1
    st.d $r6, $sp, LA_TF_A2
    st.d $r7, $sp, LA_TF_A3
    st.d $r8, $sp, LA_TF_A4
    st.d $r9, $sp, LA_TF_A5
    st.d $r10, $sp, LA_TF_A6
    st.d $r11, $sp, LA_TF_A7
    st.d $r13, $sp, LA_TF_T1
    st.d $r14, $sp, LA_TF_T2
    st.d $r15, $sp, LA_TF_T3
    st.d $r16, $sp, LA_TF_T4
    st.d $r17, $sp, LA_TF_T5
    st.d $r18, $sp, LA_TF_T6
    st.d $r19, $sp, LA_TF_T7
    st.d $r20, $sp, LA_TF_T8
    st.d $r21, $sp, LA_TF_U0
    st.d $r22, $sp, LA_TF_FP
    st.d $r23, $sp, LA_TF_S0
    st.d $r24, $sp, LA_TF_S1
    st.d $r25, $sp, LA_TF_S2
    st.d $r26, $sp, LA_TF_S3
    st.d $r27, $sp, LA_TF_S4
    st.d $r28, $sp, LA_TF_S5
    st.d $r29, $sp, LA_TF_S6
    st.d $r30, $sp, LA_TF_S7
    st.d $r31, $sp, LA_TF_S8
    csrrd $r12, CSR_ERA
    st.d $r12, $sp, LA_TF_ERA
    csrrd $r12, CSR_PRMD
    st.d $r12, $sp, LA_TF_PRMD
    csrrd $r12, CSR_BADV
    st.d $r12, $sp, LA_TF_BADV
    csrrd $r12, CSR_ESTAT
    st.d $r12, $sp, LA_TF_ESTAT
    move $a0, $sp
    bl arch_runtime_trap_handler
    # C++ 可修改返回地址和前态，因此先回写 CSR，再恢复通用寄存器。
    ld.d $r12, $sp, LA_TF_ERA
    csrwr $r12, CSR_ERA
    ld.d $r12, $sp, LA_TF_PRMD
    csrwr $r12, CSR_PRMD
    andi $r12, $r12, PRMD_PPLV_MASK
    beqz $r12, .Lloongarch_save0_ready
    addi.d $r12, $sp, LA_TF_SIZE
    csrwr $r12, CSR_SAVE0
.Lloongarch_save0_ready:
    ld.d $r1, $sp, LA_TF_RA
    ld.d $r2, $sp, LA_TF_TP
    ld.d $r4, $sp, LA_TF_A0
    ld.d $r5, $sp, LA_TF_A1
    ld.d $r6, $sp, LA_TF_A2
    ld.d $r7, $sp, LA_TF_A3
    ld.d $r8, $sp, LA_TF_A4
    ld.d $r9, $sp, LA_TF_A5
    ld.d $r10, $sp, LA_TF_A6
    ld.d $r11, $sp, LA_TF_A7
    ld.d $r12, $sp, LA_TF_T0
    ld.d $r13, $sp, LA_TF_T1
    ld.d $r14, $sp, LA_TF_T2
    ld.d $r15, $sp, LA_TF_T3
    ld.d $r16, $sp, LA_TF_T4
    ld.d $r17, $sp, LA_TF_T5
    ld.d $r18, $sp, LA_TF_T6
    ld.d $r19, $sp, LA_TF_T7
    ld.d $r20, $sp, LA_TF_T8
    ld.d $r21, $sp, LA_TF_U0
    ld.d $r22, $sp, LA_TF_FP
    ld.d $r23, $sp, LA_TF_S0
    ld.d $r24, $sp, LA_TF_S1
    ld.d $r25, $sp, LA_TF_S2
    ld.d $r26, $sp, LA_TF_S3
    ld.d $r27, $sp, LA_TF_S4
    ld.d $r28, $sp, LA_TF_S5
    ld.d $r29, $sp, LA_TF_S6
    ld.d $r30, $sp, LA_TF_S7
    ld.d $r31, $sp, LA_TF_S8
    # 最后恢复被中断现场的 sp，TrapFrame 自此不可再访问。
    ld.d $sp, $sp, LA_TF_SP
    ertn

    .align 12
    .global _loongarch_kernel_tlb_refill
    .type _loongarch_kernel_tlb_refill, @function
_loongarch_kernel_tlb_refill:
    # 重填异常不建立完整 TrapFrame，只借用 t0 并通过专用 CSR 保存。
    csrwr $t0, CSR_TLBRSAVE
    csrrd $t0, CSR_PGD
    lddir $t0, $t0, 3
    lddir $t0, $t0, 2
    lddir $t0, $t0, 1
    ldpte $t0, 0
    ldpte $t0, 1
    tlbfill
    csrrd $t0, CSR_TLBRSAVE
    ertn
    .size _loongarch_kernel_tlb_refill, . - _loongarch_kernel_tlb_refill
