/**
 * @file trap.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 异常处理程序入口, 出口汇编代码
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

STACK_SIZE = 272

.section .text
.globl isr_entry
.extern handle_trap
/* ISR入口 */
isr_entry:
save_context:
    /* 第1步: 切换到内核栈空间 */
    /* 按约定, sscratch存放了该线程的内核栈顶地址, 因此直接交换sp和sscratch即可切换到内核栈空间 */
    csrrw sp, sscratch, sp
    /* 但如果sp为零, 说明之前是S-Mode, 否则是U-Mode, 进入不同情况对栈寄存器的处理 */
    bnez sp, isr_ustks
/* S-Mode Stack Save*/
isr_sstks:
    /* 第2步: 从sscratch中恢复sp */
    /* 此时sp = 0, sscratch中保存的就是原sp, 直接交换即可恢复sp */
    csrrw sp, sscratch, sp
    /* 分配栈空间并保存x1, x2 */
    addi sp, sp, -STACK_SIZE
    sd x1, 0(sp)
    /* 令 x1 指向原栈顶, 即原x2(sp) */
    addi x1, sp, STACK_SIZE
    /* 保存x2 */
    sd x1, 8(sp)
    j isr_sregs
/* U-Mode Stack Save*/
isr_ustks:
    /* 此时, sp 为内核栈顶地址 */
    /* 分配栈空间并保存x1, x2 */
    addi sp, sp, -STACK_SIZE
    sd x1, 0(sp)
    /* 令 x1 指向原栈顶, 即原x2(sp) */
    csrr x1, sscratch
    /* 保存 x2 */
    sd x1, 8(sp)
    /* 此时将sscratch设置为原内核栈顶地址(依约定, sscratch中保存的就是内核栈顶地址) */
    /* 原内核栈顶 - STACK_SIZE = sp => 原内核栈顶 = sp + STACK_SIZE */
    addi x1, sp, STACK_SIZE
    csrw sscratch, x1
/* 通用寄存器保存(除了x1, x2) */
isr_sregs:
    sd x3, 16(sp)
    sd x4, 24(sp)
    sd x5, 32(sp)
    sd x6, 40(sp)
    sd x7, 48(sp)
    sd x8, 56(sp)
    sd x9, 64(sp)
    sd x10, 72(sp)
    sd x11, 80(sp)
    sd x12, 88(sp)
    sd x13, 96(sp)
    sd x14, 104(sp)
    sd x15, 112(sp)
    sd x16, 120(sp)
    sd x17, 128(sp)
    sd x18, 136(sp)
    sd x19, 144(sp)
    sd x20, 152(sp)
    sd x21, 160(sp)
    sd x22, 168(sp)
    sd x23, 176(sp)
    sd x24, 184(sp)
    sd x25, 192(sp)
    sd x26, 200(sp)
    sd x27, 208(sp)
    sd x28, 216(sp)
    sd x29, 224(sp)
    sd x30, 232(sp)
    sd x31, 240(sp)
    /* 在这之后, 通用寄存器就任我们摆布了 */
isr_scsrs:
    /* 保存关键 CSR */
    csrr t0, sepc
    sd t0, 248(sp)
    csrr t0, sstatus
    sd t0, 256(sp)
    /* 跳转至C异常处理函数 */
    j __handle_trap

__handle_trap:
    /* 传参 */
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    mv   a3, sp
    call handle_trap
    /* 处理完成后, 跳转至ISR出口 */
    j isr_exit

isr_exit:
restore_context:
    /* 理论上, 此时的sp就是我们的内核栈sp了 */
    /* 然而, 此时的sp可能并非sscratch中保存的内核栈顶地址 */
    /* 我们将再次从sscratch中恢复sp. 这样, 内核就可以通过操纵sscratch来控制sp */
    csrrw sp, sscratch, sp
    /* 然而, 若此时sp为0, 则sscratch原先即为0, 即原先并不存在所谓内核栈 */
    /* 因此, 如果sp为0, 我们要从sscratch中恢复sp */
    /* 否则, sp = sscratch - STACK_SIZE */
    bnez sp, isr_ustkr
/* S-Mode Stack Restore */
isr_sstkr:
    /* 此时, sp = 0, sscratch中保存的就是原sp, 直接交换即可恢复sp */
    csrrw sp, sscratch, sp
    /* 恢复通用寄存器 */
    j isr_rcsrs
/* U-Mode Stack Restore */
isr_ustkr:
    /* 此时, sp = 内核栈顶地址 */
    /* 我们需要恢复sscratch, 使其依然指向内核栈顶地址 */
    csrw sscratch, sp
    /* sp 减去STACK_SIZE就是保存寄存器的栈底地址 */
    addi sp, sp, -STACK_SIZE
/* CSR恢复 */
isr_rcsrs:
    ld t0, 256(sp)
    csrw sstatus, t0
    ld t0, 248(sp)
    csrw sepc, t0
/* 通用寄存器恢复(除x2(sp)) */
isr_rregs:
    ld x1, 0(sp)
    ld x3, 16(sp)
    ld x4, 24(sp)
    ld x5, 32(sp)
    ld x6, 40(sp)
    ld x7, 48(sp)
    ld x8, 56(sp)
    ld x9, 64(sp)
    ld x10, 72(sp)
    ld x11, 80(sp)
    ld x12, 88(sp)
    ld x13, 96(sp)
    ld x14, 104(sp)
    ld x15, 112(sp)
    ld x16, 120(sp)
    ld x17, 128(sp)
    ld x18, 136(sp)
    ld x19, 144(sp)
    ld x20, 152(sp)
    ld x21, 160(sp)
    ld x22, 168(sp)
    ld x23, 176(sp)
    ld x24, 184(sp)
    ld x25, 192(sp)
    ld x26, 200(sp)
    ld x27, 208(sp)
    ld x28, 216(sp)
    ld x29, 224(sp)
    ld x30, 232(sp)
    ld x31, 240(sp)
isr_rtn:
    /* 直接从当前内核栈中恢复sp */
    ld sp, 8(sp)
    /* 此时, sp指向了之前的栈顶指针 */
    /* 上下文此时已彻底恢复 */
    sret
