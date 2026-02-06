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

.section .text
.globl isr_entry
.extern handle_trap
isr_entry:
    /* 第1步：切换栈空间 */
    csrrw sp, sscratch, sp
    /* 如果sp为零，说明之前是S-Mode, 否则是U-Mode, 进入不同情况对栈寄存器的处理 */
    bne sp, zero, isr_ustkp
    /* S-Mode Stack Protection*/
isr_sstkp:
    /* 第2步: 从sscratch中恢复sp */
    csrrw sp, sscratch, sp
    /* 分配栈空间并保存x1, x2 */
    addi sp, sp, -264
    sd x1, 0(sp)
    /* 令 x1 指向原栈顶, 即原x2(sp) */
    addi x1, sp, 264
    /* 保存x2 */
    sd x1, 8(sp)
    j isr_regp
    /* U-Mode Stack Protection*/
isr_ustkp:
    /* 此时, sp 为内核栈顶地址 */
    /* 分配栈空间并保存x1, x2 */
    addi sp, sp, -264
    sd x1, 0(sp)
    /* 令 x1 指向原栈顶, 即原x2(sp) */
    csrr x1, sscratch
    /* 保存 x2 */
    sd x1, 8(sp)
    /* 清空sscratch */
    csrw sscratch, zero
isr_regp:
    /* 第3步: 保存通用寄存器 */
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
    /* 接下来寄存器就可以随意使用了 */
    /* 保存关键 CSR */
    csrr t0, sepc
    sd t0, 248(sp)
    csrr t0, sstatus
    sd t0, 256(sp)
    /* 传参 */
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    mv   a3, sp
    /* 调用C异常处理函数 */
    call handle_trap
    /* 判断sstatus的SPP位，决定返回U-Mode还是S-Mode */
    /* 读取sstatus */
    csrr t0, sstatus
    /* 检查SPP位 */
    andi t1, t0, 0x100
    /* 根据结果选择栈恢复路径 */
    beqz t1, isr_regr
    /* 此时 t1 != 0, 说明之前是S-Mode */
    /* 将8(sp), 即原sp设置为0 */
    sd zero, 8(sp)
    /* 恢复栈空间 */
isr_regr:
    /* 恢复关键 CSR */
    ld t0, 256(sp)
    csrw sstatus, t0
    ld t0, 248(sp)
    csrw sepc, t0
    /* 接下来是特殊的对sscratch的处理 */
    ld x1, 8(sp)
    /* 这里, 被处理过的sp值将被写入sscratch中 */
    csrw sscratch, x1
    /* 恢复通用寄存器 */
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
    /* 恢复栈空间 */
    addi sp, sp, 264
    /* 第4步: 切换回原栈空间 */
    csrrw sp, sscratch, sp
    /* 如果 sp 为0, 说明要返回 S-Mode, 那么此时sscratch中保存的就是原sp */
    /* 否则返回 U-Mode, sscratch中保存的就是内核栈顶地址 */
    bnez sp, isr_rtn
    /* 再次从sscratch中恢复sp */
    csrrw sp, sscratch, sp
isr_rtn:
    sret
