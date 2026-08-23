/**
 * @file smp_entry.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LABOOT LoongArch AP 汇编入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

/* 常驻 LABOOT AP 入口。兼容固件通过 MBUF1 传入 ApStartupData
 * 的物理地址。 */
#include <arch/loongarch64/valdef.h>

.section .smp.trampoline, "ax", @progbits
.global laboot_secondary_trampoline
.type laboot_secondary_trampoline, @function
laboot_secondary_trampoline:
    li.w    $t0, IOCSR_MBUF1
    iocsrrd.d $t6, $t0

    /* 先安装启动器准备好的页表格式与根地址，当前阶段仍按物理地址读取参数。 */
    ld.d    $t1, $t6, 0
    csrwr   $t1, CSR_PWCTL0
    ld.d    $t1, $t6, 8
    csrwr   $t1, CSR_PWCTL1
    ld.d    $t1, $t6, 16
    csrwr   $t1, CSR_STLBPGSIZE
    ld.d    $t1, $t6, 24
    csrwr   $t1, CSR_PGDL
    ld.d    $t1, $t6, 32
    csrwr   $t1, CSR_PGDH
    invtlb  0, $zero, $zero

    ld.d    $t1, $t6, 40
    csrwr   $t1, CSR_DMWIN0
    ld.d    $t1, $t6, 48
    csrwr   $t1, CSR_DMWIN1
    ld.d    $t1, $t6, 56
    csrwr   $t1, CSR_DMWIN2
    ld.d    $t1, $t6, 64
    csrwr   $t1, CSR_DMWIN3
    ld.d    $t1, $t6, 72
    csrwr   $t1, CSR_TLBRENTRY

    /* 分页切换前一次性装载高半区栈、CpuLocal、参数和 C++ 入口。 */
    ld.d    $sp, $t6, 80
    ld.d    $tp, $t6, 88
    ld.d    $a0, $t6, 96
    ld.d    $t2, $t6, 104

    li.w    $t3, CRMD_PG
    li.w    $t4, CRMD_DA | CRMD_PG | CRMD_IE
    csrxchg $t3, $t4, CSR_CRMD
    /* 屏障保证后续跳转按新页表和新取指视图执行。 */
    ibar    0
    dbar    0
    jirl    $zero, $t2, 0
.size laboot_secondary_trampoline, .-laboot_secondary_trampoline
