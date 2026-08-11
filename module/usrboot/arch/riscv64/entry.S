/**
 * @file entry.S
 * @brief RISC-V64 usrboot 入口与启动栈
 * @version 0.1.0-dev.1
 * @date 2026-08-11
 *
 * @copyright Copyright (c) 2026
 */

.section .text, "ax", @progbits
.p2align 2
.global _start
.type _start, @function
.extern __early_main
_start:
    .option push
    .option norelax
    la      gp, __global_pointer$
    la      sp, __usrboot_stack_top
    .option pop

    call    __early_main

.Lhalt:
    j       .Lhalt
.size _start, .-_start

/*
 * __early_main 会清零 BSS，因此启动栈必须位于 PROGBITS data 段，不能放入
 * NOBITS BSS。栈向低地址增长，栈顶满足 RISC-V psABI 的 16 字节对齐要求。
 */
.section .data, "aw", @progbits
.balign 16
.global __usrboot_stack_bottom
.type __usrboot_stack_bottom, @object
__usrboot_stack_bottom:
    .zero   65536
.global __usrboot_stack_top
__usrboot_stack_top:
.size __usrboot_stack_bottom, __usrboot_stack_top-__usrboot_stack_bottom
