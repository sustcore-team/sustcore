#include <boot/laboot/macros.h>

    .section .laboot.text, "ax", @progbits

    .extern _laboot_setup

    .globl _la_start
    .type _la_start, @function
_la_start:
    la.abs $sp, __la_boot_stack_top
    call _laboot_setup

_la_paging_setup:
    move $s0, $a0

    ld.d $t0, $s0, LABOOT_SETUP_PWCTL0_OFFSET
    csrwr $t0, CSR_PWCTL0
    ld.d $t0, $s0, LABOOT_SETUP_PWCTL1_OFFSET
    csrwr $t0, CSR_PWCTL1
    ld.d $t0, $s0, LABOOT_SETUP_STLBPGSIZE_OFFSET
    csrwr $t0, CSR_STLBPGSIZE

    ld.d $t0, $s0, LABOOT_SETUP_PGDL_OFFSET
    csrwr $t0, CSR_PGDL
    ld.d $t0, $s0, LABOOT_SETUP_PGDH_OFFSET
    csrwr $t0, CSR_PGDH
    invtlb 0, $zero, $zero

    ld.d $t0, $s0, LABOOT_SETUP_DMW0_OFFSET
    csrwr $t0, CSR_DMWIN0
    ld.d $t0, $s0, LABOOT_SETUP_DMW1_OFFSET
    csrwr $t0, CSR_DMWIN1
    ld.d $t0, $s0, LABOOT_SETUP_DMW2_OFFSET
    csrwr $t0, CSR_DMWIN2
    ld.d $t0, $s0, LABOOT_SETUP_DMW3_OFFSET
    csrwr $t0, CSR_DMWIN3

    ld.d $t0, $s0, LABOOT_SETUP_TLBRENTRY_OFFSET
    csrwr $t0, CSR_TLBRENTRY

    li.w $t0, 0x1c
    ld.d $t1, $s0, LABOOT_SETUP_CRMD_VALUE_OFFSET
    csrxchg $t1, $t0, CSR_CRMD
    ibar 0
    dbar 0

_la_trampoline:
    ld.d $t0, $s0, LABOOT_SETUP_POST_ENTRY_OFFSET
    jr $t0

.align 12
.global _laboot_tlb_refill
.type _laboot_tlb_refill, @function
_laboot_tlb_refill:
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

    .section .laboot.data, "aw", @progbits
.align 12
__la_boot_stack:
    .skip 8 * 1024
.global __la_boot_stack_top
__la_boot_stack_top:
