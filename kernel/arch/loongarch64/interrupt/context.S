/**
 * @file context.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 线程调度上下文切换
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/context_offsets.h>
#include <arch/loongarch64/valdef.h>
#include <arch/loongarch64/interrupt/offsets.h>

    .section .text.scheduler.context, "ax", @progbits
    .align 4
    .global __switch_to
    .type __switch_to, @function
__switch_to:
    # a0 保存旧线程的被调用者保存寄存器，包含恢复该调用点所需的 ra/sp。
    st.d $ra, $a0, LA_CTX_RA
    st.d $sp, $a0, LA_CTX_SP
    st.d $fp, $a0, LA_CTX_FP
    st.d $s0, $a0, LA_CTX_S0
    st.d $s1, $a0, LA_CTX_S1
    st.d $s2, $a0, LA_CTX_S2
    st.d $s3, $a0, LA_CTX_S3
    st.d $s4, $a0, LA_CTX_S4
    st.d $s5, $a0, LA_CTX_S5
    st.d $s6, $a0, LA_CTX_S6
    st.d $s7, $a0, LA_CTX_S7
    st.d $s8, $a0, LA_CTX_S8

    # 从 a1 切换栈和返回地址；jr 将回到新线程上次停下的位置或 bootstrap。
    ld.d $ra, $a1, LA_CTX_RA
    ld.d $sp, $a1, LA_CTX_SP
    ld.d $fp, $a1, LA_CTX_FP
    ld.d $s0, $a1, LA_CTX_S0
    ld.d $s1, $a1, LA_CTX_S1
    ld.d $s2, $a1, LA_CTX_S2
    ld.d $s3, $a1, LA_CTX_S3
    ld.d $s4, $a1, LA_CTX_S4
    ld.d $s5, $a1, LA_CTX_S5
    ld.d $s6, $a1, LA_CTX_S6
    ld.d $s7, $a1, LA_CTX_S7
    ld.d $s8, $a1, LA_CTX_S8
    jr $ra
    .size __switch_to, . - __switch_to

    .global _ZN11loongarch643hal10enter_userERNS0_9TrapFrameEm
    .type _ZN11loongarch643hal10enter_userERNS0_9TrapFrameEm, @function
_ZN11loongarch643hal10enter_userERNS0_9TrapFrameEm:
    # a0 指向初始 TrapFrame；SAVE0 保存用户异常进入时要切换到的内核栈顶。
    move $t8, $a1
    csrwr $t8, CSR_SAVE0
    move $t8, $a0
    ld.d $t0, $t8, LA_TF_ERA
    csrwr $t0, CSR_ERA
    ld.d $t0, $t8, LA_TF_PRMD
    csrwr $t0, CSR_PRMD
    ld.d $r1, $t8, LA_TF_RA
    ld.d $r2, $t8, LA_TF_TP
    ld.d $sp, $t8, LA_TF_SP
    ld.d $r4, $t8, LA_TF_A0
    ld.d $r5, $t8, LA_TF_A1
    ld.d $r6, $t8, LA_TF_A2
    ld.d $r7, $t8, LA_TF_A3
    ld.d $r8, $t8, LA_TF_A4
    ld.d $r9, $t8, LA_TF_A5
    ld.d $r10, $t8, LA_TF_A6
    ld.d $r11, $t8, LA_TF_A7
    ld.d $r12, $t8, LA_TF_T0
    ld.d $r13, $t8, LA_TF_T1
    ld.d $r14, $t8, LA_TF_T2
    ld.d $r15, $t8, LA_TF_T3
    ld.d $r16, $t8, LA_TF_T4
    ld.d $r17, $t8, LA_TF_T5
    ld.d $r18, $t8, LA_TF_T6
    ld.d $r19, $t8, LA_TF_T7
    ld.d $r21, $t8, LA_TF_U0
    ld.d $r22, $t8, LA_TF_FP
    ld.d $r23, $t8, LA_TF_S0
    ld.d $r24, $t8, LA_TF_S1
    ld.d $r25, $t8, LA_TF_S2
    ld.d $r26, $t8, LA_TF_S3
    ld.d $r27, $t8, LA_TF_S4
    ld.d $r28, $t8, LA_TF_S5
    ld.d $r29, $t8, LA_TF_S6
    ld.d $r30, $t8, LA_TF_S7
    ld.d $r31, $t8, LA_TF_S8
    ld.d $r20, $t8, LA_TF_T8
    ertn
    .size _ZN11loongarch643hal10enter_userERNS0_9TrapFrameEm, . - _ZN11loongarch643hal10enter_userERNS0_9TrapFrameEm
