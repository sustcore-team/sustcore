/**
 * @file entry.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 内核高半区汇编入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/valdef.h>

    .section .text.entry, "ax", @progbits

    .extern __bsp_early_main
    .extern __bsp_stack_top

    .global __bsp_start
    .type __bsp_start, @function
__bsp_start:
    # C++ 运行环境尚未建立，先屏蔽中断并切换到内核启动栈。
    li.w $t0, CRMD_IE
    csrxchg $zero, $t0, CSR_CRMD

    la.local $sp, __bsp_stack_top
    b __bsp_early_main

    .size __bsp_start, . - __bsp_start
