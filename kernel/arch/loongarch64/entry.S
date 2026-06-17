#include <arch/loongarch64/csrnum.h>
#include <arch/loongarch64/ctxlayout.h>
#include <boot/laboot/bootinfo.h>

    .equ KERNEL_STACK_BYTES, 8 * 1024

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

    .globl __switch_to
    .type __switch_to, @function
__switch_to:
    st.d $ra, $a0, CTX_SLOT_OFFSET(CTX_RA_SLOT)
    st.d $sp, $a0, CTX_SLOT_OFFSET(CTX_SP_SLOT)
    st.d $tp, $a0, CTX_SLOT_OFFSET(CTX_TP_SLOT)
    st.d $fp, $a0, CTX_SLOT_OFFSET(CTX_FP_SLOT)
    st.d $s0, $a0, CTX_SLOT_OFFSET(CTX_S0_SLOT)
    st.d $s1, $a0, CTX_SLOT_OFFSET(CTX_S1_SLOT)
    st.d $s2, $a0, CTX_SLOT_OFFSET(CTX_S2_SLOT)
    st.d $s3, $a0, CTX_SLOT_OFFSET(CTX_S3_SLOT)
    st.d $s4, $a0, CTX_SLOT_OFFSET(CTX_S4_SLOT)
    st.d $s5, $a0, CTX_SLOT_OFFSET(CTX_S5_SLOT)
    st.d $s6, $a0, CTX_SLOT_OFFSET(CTX_S6_SLOT)
    st.d $s7, $a0, CTX_SLOT_OFFSET(CTX_S7_SLOT)
    st.d $s8, $a0, CTX_SLOT_OFFSET(CTX_S8_SLOT)

    ld.d $ra, $a1, CTX_SLOT_OFFSET(CTX_RA_SLOT)
    ld.d $sp, $a1, CTX_SLOT_OFFSET(CTX_SP_SLOT)
    ld.d $tp, $a1, CTX_SLOT_OFFSET(CTX_TP_SLOT)
    ld.d $fp, $a1, CTX_SLOT_OFFSET(CTX_FP_SLOT)
    ld.d $s0, $a1, CTX_SLOT_OFFSET(CTX_S0_SLOT)
    ld.d $s1, $a1, CTX_SLOT_OFFSET(CTX_S1_SLOT)
    ld.d $s2, $a1, CTX_SLOT_OFFSET(CTX_S2_SLOT)
    ld.d $s3, $a1, CTX_SLOT_OFFSET(CTX_S3_SLOT)
    ld.d $s4, $a1, CTX_SLOT_OFFSET(CTX_S4_SLOT)
    ld.d $s5, $a1, CTX_SLOT_OFFSET(CTX_S5_SLOT)
    ld.d $s6, $a1, CTX_SLOT_OFFSET(CTX_S6_SLOT)
    ld.d $s7, $a1, CTX_SLOT_OFFSET(CTX_S7_SLOT)
    ld.d $s8, $a1, CTX_SLOT_OFFSET(CTX_S8_SLOT)
    jr $ra

    .globl new_utask_trampoline
    .type new_utask_trampoline, @function
new_utask_trampoline:
    move $sp, $s0
    ld.d $t0, $sp, CTX_SLOT_OFFSET(CTX_ERA_SLOT)
    ld.d $t1, $sp, CTX_SLOT_OFFSET(CTX_SP_SLOT)
    move $sp, $t1
    move $a0, $t0
    move $a1, $t1
    b new_utask_trampoline_hang

new_utask_trampoline_hang:
    b new_utask_trampoline_hang

    .section .bss.stack, "aw", @nobits
    .align 12
boot_stack:
    .space KERNEL_STACK_BYTES
    .globl boot_stack_top
boot_stack_top:
