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
