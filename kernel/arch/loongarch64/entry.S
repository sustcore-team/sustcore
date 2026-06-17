#include <arch/loongarch64/csrnum.h>
#include <arch/loongarch64/ctxlayout.h>
#include <boot/laboot/bootinfo.h>

    .equ KERNEL_STACK_BYTES, 1024 * 64

    .section .data
    .extern bootinfo_ptr
    .extern c_setup_main
    .extern __hart_context

    .section .text, "ax", @progbits
    .globl _start
    .type _start, @function
_start:
    li.d $t0, MAX_HARTS
    bgeu $a0, $t0, .Linvalid_hart

    la.local $t0, __hart_context
    li.d $t1, HART_CONTEXT_SIZE
    mul.d $t1, $a0, $t1
    add.d $tp, $t0, $t1

    la.local $sp, boot_stack_top
    la.local $t0, bootinfo_ptr
    st.d $a1, $t0, 0
    bl c_setup

.Linvalid_hart:
    b .Linvalid_hart

    .globl c_setup
    .type c_setup, @function
c_setup:
    la.local $sp, boot_stack_top
    b c_setup_main

    .section .bss.stack, "aw", @nobits
    .align 12
boot_stack:
    .space KERNEL_STACK_BYTES
    .globl boot_stack_top
boot_stack_top:
