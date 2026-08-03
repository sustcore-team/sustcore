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
