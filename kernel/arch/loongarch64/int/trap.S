#include <arch/loongarch64/csrnum.h>
#include <arch/loongarch64/ctxlayout.h>

    .equ STACK_SIZE, CTX_SLOT_OFFSET(CTX_SLOT_COUNT)

    .macro ctx_store base, slot, reg
        st.d \reg, \base, CTX_SLOT_OFFSET(\slot)
    .endm

    .macro ctx_load reg, base, slot
        ld.d \reg, \base, CTX_SLOT_OFFSET(\slot)
    .endm

    .section .text, "ax", @progbits
    .globl isr_entry
    .extern handle_trap

isr_entry:
save_context:
    csrwr $t1, CSR_SAVE1
    csrwr $t2, CSR_SAVE2
    csrwr $t3, CSR_SAVE3

    csrrd $t0, CSR_SAVE0
    beqz  $t0, isr_sstks

isr_ustks:
    move  $t1, $sp
    move  $sp, $t0
    csrwr $t1, CSR_SAVE0
    addi.d $sp, $sp, -STACK_SIZE
    ctx_store $sp, CTX_SP_SLOT, $t1
    csrwr $zero, CSR_SAVE0
    b isr_sregs

isr_sstks:
    move  $t1, $sp
    addi.d $sp, $sp, -STACK_SIZE
    ctx_store $sp, CTX_SP_SLOT, $t1

isr_sregs:
    ctx_store $sp, CTX_RA_SLOT, $ra
    ctx_store $sp, CTX_TP_SLOT, $tp
    csrrd $t0, CSR_SAVE1
    ctx_store $sp, CTX_T1_SLOT, $t0
    csrrd $t0, CSR_SAVE2
    ctx_store $sp, CTX_T2_SLOT, $t0
    csrrd $t0, CSR_SAVE3
    ctx_store $sp, CTX_T3_SLOT, $t0
    ctx_store $sp, CTX_A0_SLOT, $a0
    ctx_store $sp, CTX_A1_SLOT, $a1
    ctx_store $sp, CTX_A2_SLOT, $a2
    ctx_store $sp, CTX_A3_SLOT, $a3
    ctx_store $sp, CTX_A4_SLOT, $a4
    ctx_store $sp, CTX_A5_SLOT, $a5
    ctx_store $sp, CTX_A6_SLOT, $a6
    ctx_store $sp, CTX_A7_SLOT, $a7
    ctx_store $sp, CTX_T0_SLOT, $t0
    ctx_store $sp, CTX_T4_SLOT, $t4
    ctx_store $sp, CTX_T5_SLOT, $t5
    ctx_store $sp, CTX_T6_SLOT, $t6
    ctx_store $sp, CTX_T7_SLOT, $t7
    ctx_store $sp, CTX_T8_SLOT, $t8
    ctx_store $sp, CTX_U0_SLOT, $r21
    ctx_store $sp, CTX_FP_SLOT, $fp
    ctx_store $sp, CTX_S0_SLOT, $s0
    ctx_store $sp, CTX_S1_SLOT, $s1
    ctx_store $sp, CTX_S2_SLOT, $s2
    ctx_store $sp, CTX_S3_SLOT, $s3
    ctx_store $sp, CTX_S4_SLOT, $s4
    ctx_store $sp, CTX_S5_SLOT, $s5
    ctx_store $sp, CTX_S6_SLOT, $s6
    ctx_store $sp, CTX_S7_SLOT, $s7
    ctx_store $sp, CTX_S8_SLOT, $s8

isr_scsrs:
    csrrd $t0, CSR_ERA
    ctx_store $sp, CTX_ERA_SLOT, $t0
    csrrd $t0, CSR_ESTAT
    ctx_store $sp, CTX_ESTAT_SLOT, $t0

__handle_trap:
    ctx_load $a0, $sp, CTX_ERA_SLOT
    ctx_load $a1, $sp, CTX_ESTAT_SLOT
    move $a2, $sp
    bl handle_trap
    b isr_exit

isr_exit:
restore_context:
    ctx_load $t0, $sp, CTX_ERA_SLOT
    csrwr $t0, CSR_ERA

isr_rregs:
    ctx_load $ra, $sp, CTX_RA_SLOT
    ctx_load $tp, $sp, CTX_TP_SLOT
    ctx_load $a0, $sp, CTX_A0_SLOT
    ctx_load $a1, $sp, CTX_A1_SLOT
    ctx_load $a2, $sp, CTX_A2_SLOT
    ctx_load $a3, $sp, CTX_A3_SLOT
    ctx_load $a4, $sp, CTX_A4_SLOT
    ctx_load $a5, $sp, CTX_A5_SLOT
    ctx_load $a6, $sp, CTX_A6_SLOT
    ctx_load $a7, $sp, CTX_A7_SLOT
    ctx_load $t0, $sp, CTX_T0_SLOT
    ctx_load $t1, $sp, CTX_T1_SLOT
    ctx_load $t2, $sp, CTX_T2_SLOT
    ctx_load $t3, $sp, CTX_T3_SLOT
    ctx_load $t4, $sp, CTX_T4_SLOT
    ctx_load $t5, $sp, CTX_T5_SLOT
    ctx_load $t6, $sp, CTX_T6_SLOT
    ctx_load $t7, $sp, CTX_T7_SLOT
    ctx_load $t8, $sp, CTX_T8_SLOT
    ctx_load $r21, $sp, CTX_U0_SLOT
    ctx_load $fp, $sp, CTX_FP_SLOT
    ctx_load $s0, $sp, CTX_S0_SLOT
    ctx_load $s1, $sp, CTX_S1_SLOT
    ctx_load $s2, $sp, CTX_S2_SLOT
    ctx_load $s3, $sp, CTX_S3_SLOT
    ctx_load $s4, $sp, CTX_S4_SLOT
    ctx_load $s5, $sp, CTX_S5_SLOT
    ctx_load $s6, $sp, CTX_S6_SLOT
    ctx_load $s7, $sp, CTX_S7_SLOT
    ctx_load $s8, $sp, CTX_S8_SLOT

isr_rtn:
    ctx_load $sp, $sp, CTX_SP_SLOT
    ertn

    .globl new_utask_trampoline
    .type new_utask_trampoline, @function
new_utask_trampoline:
    move $sp, $s0
    b restore_context

    .globl __switch_to
    .type __switch_to, @function
__switch_to:
    ctx_store $a0, CTX_RA_SLOT, $ra
    ctx_store $a0, CTX_SP_SLOT, $sp
    ctx_store $a0, CTX_TP_SLOT, $tp
    ctx_store $a0, CTX_FP_SLOT, $fp
    ctx_store $a0, CTX_S0_SLOT, $s0
    ctx_store $a0, CTX_S1_SLOT, $s1
    ctx_store $a0, CTX_S2_SLOT, $s2
    ctx_store $a0, CTX_S3_SLOT, $s3
    ctx_store $a0, CTX_S4_SLOT, $s4
    ctx_store $a0, CTX_S5_SLOT, $s5
    ctx_store $a0, CTX_S6_SLOT, $s6
    ctx_store $a0, CTX_S7_SLOT, $s7
    ctx_store $a0, CTX_S8_SLOT, $s8

    ctx_load $ra, $a1, CTX_RA_SLOT
    ctx_load $sp, $a1, CTX_SP_SLOT
    ctx_load $tp, $a1, CTX_TP_SLOT
    ctx_load $fp, $a1, CTX_FP_SLOT
    ctx_load $s0, $a1, CTX_S0_SLOT
    ctx_load $s1, $a1, CTX_S1_SLOT
    ctx_load $s2, $a1, CTX_S2_SLOT
    ctx_load $s3, $a1, CTX_S3_SLOT
    ctx_load $s4, $a1, CTX_S4_SLOT
    ctx_load $s5, $a1, CTX_S5_SLOT
    ctx_load $s6, $a1, CTX_S6_SLOT
    ctx_load $s7, $a1, CTX_S7_SLOT
    ctx_load $s8, $a1, CTX_S8_SLOT
    jr $ra

    .globl loongarch64_trap_return
    .type loongarch64_trap_return, @function
loongarch64_trap_return:
    b restore_context
