/**
 * @file smp_entry.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI RISC-V AP 汇编入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

/* 常驻 SBI AP 入口。SBI HSM 在 a0 传 hart ID，在 a1 传 ApBootArgs
 * 的物理地址。 */
.section .smp.trampoline, "ax", @progbits
.global sbi_secondary_trampoline
.type sbi_secondary_trampoline, @function
sbi_secondary_trampoline:
    /* 分页开启前先取出全部高半区目标，避免切换后继续解引用物理参数指针。 */
    mv      t6, a1
    ld      t0, 24(t6)             /* root_pt */
    ld      t4, 32(t6)             /* high-half stack_top */
    ld      t5, 40(t6)             /* high-half CpuLocal */
    ld      t3, 48(t6)             /* high-half C++ entry */
1:  auipc   t2, %pcrel_hi(sbi_secondary_after_mmu)
    addi    t2, t2, %pcrel_lo(1b)
    li      t1, 0xffffffff00000000
    add     t2, t2, t1
    add     t6, t6, t1
    srli    t1, t0, 12
    li      t0, 0x8000000000000000 /* Sv39 */
    or      t1, t1, t0
    csrw    satp, t1
    sfence.vma zero, zero
    /* 当前 PC 和参数指针同步加上高半区偏移，再离开恒等映射执行流。 */
    jr      t2

sbi_secondary_after_mmu:
    /* 高半区就绪后绑定内核栈和 CpuLocal，按 C++ ABI 传递启动参数。 */
    mv      sp, t4
    mv      tp, t5
    mv      a0, t6
    jr      t3
.size sbi_secondary_trampoline, .-sbi_secondary_trampoline
