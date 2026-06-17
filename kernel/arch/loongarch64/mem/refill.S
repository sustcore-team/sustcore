/**
 * @file refill.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 TLB refill 入口
 * @version alpha-1.0.0
 * @date 2026-06-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/mem/paging.h>
#include <arch/loongarch64/csrnum.h>

    .section .text, "ax", @progbits

    .globl loongarch64_tlb_refill_entry
    .type loongarch64_tlb_refill_entry, @function
    .align 12
loongarch64_tlb_refill_entry:
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
