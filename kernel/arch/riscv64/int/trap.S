/**
 * @file trap.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 异常处理程序入口, 出口汇编代码
 * @version alpha-1.0.0
 * @date 2025-11-18
 * 该文件实际上并不会被编译, 只是作为trap.h中内联汇编代码的参考
 * @copyright Copyright (c) 2025
 * 
 */

entry:
    /* 第1步：切换栈空间 */
    csrrw sp, sscratch, sp
    /* 如果sp为零，说明之前是S-Mode, 否则是U-Mode, 进入不同情况对栈寄存器的处理 */
    bne sp, zero, name_entry_user_stkproc
name_entry_kernel_stkproc:
    /* 第2.1步: 内核态对栈寄存器的处理 */
    /* 从sscratch中恢复sp（即之前的内核栈） */
    csrrw sp, sscratch, sp
    /* 为了与user_trap保持一致, 先分配栈空间并保存一部分寄存器(x1, x2) */
    addi sp, sp, -264
    sd x1,  0(sp)
    /* 我们约定x2是进入isr前的x2(sp), 于是可以先利用已经被保存的x1 */
    addi x1, sp, 264
    sd x1,  8(sp)
    /* 进入剩余寄存器的保存 */
    j name_entry_rest_context_save
name_entry_user_stkproc:
    /* 第2.2步: 用户态对栈寄存器的处理 */
    /* 如果之前在U-Mode，则清空sscratch, 说明已经处于S-Mode */
    /* 在清空前, 把其值(在经过交换后, 此时sscratch是U-Mode下的sp, x2为S-Mode下的sp)保存到栈上 */
    /* 于是, 无论如何 我们先分配栈空间保存一部分寄存器(x1, x2) */
    addi sp, sp, -264
    sd x1,  0(sp)
    /* 我们约定x2是进入isr前的x2(sp), 我们利用x1读取sscratch并保存sp */
    csrr x1, sscratch
    sd x1,  8(sp)
    /* 清空sscratch */
    csrw sscratch, x0
    /* 进入剩余寄存器的保存 */
name_entry_rest_context_save:
    /* 第3步: 保存剩下的通用寄存器 */
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
    /* 第4步: 保存关键CSR寄存器*/
    csrr t0, sepc /* spec */
    sd t0, 248(sp)
    csrr t0, sstatus /* sstatus */
    sd t0, 256(sp)
    /* 第5步: 传参 */
    csrr %0, scause
    csrr %1, sepc
    csrr %2, stval
    mv   %3, sp
    /* 为ISR预留栈空间 */
    addi sp, sp, -512

exit:
    /* 第0步：恢复sp */
    mv   sp, %0
    /* 第1步：恢复关键CSR寄存器 */
    ld t0, 256(sp)             /* 恢复 sstatus */
    csrw sstatus, t0
    ld t0, 248(sp)             /* 恢复 sepc */
    csrw sepc, t0
    /* 第2步: 对sscratch的特殊处理 */
    /* 对isr的要求是, 在进入退出程序前, 判断是否是从S-Mode进入的还是从U-Mode进入的, 并据此修改栈上保存的sp值 */
    /* 我们将把sp值直接保存到sscratch上. 因此判断是从S-Mode进入时, 应将sp值归零 */
    ld x1, 8(sp)
    csrw sscratch, x1
    /* 第3步：恢复所有通用寄存器 */
    ld x1, 0(sp)
    /* 其中x2即为sp, 经过了上面的特殊处理 */
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
    /* 第4步: 堆栈平衡. */
    addi sp, sp, 264
    /* 第5步：最后从sscratch中恢复栈指针(sp). 此时sp指向S-Mode堆栈, sscratch指向U-Mode堆栈. 交换即可 */
    csrrw sp, sscratch, sp
    /* 第6步：梅开二度. 如果sp此时的sp为空, 说明是从S-Mode进入的ISR.  */
    bne sp, zero, name_exit
    /* 再次交换, 将sp换回 */
    csrrw sp, sscratch, sp
name_exit:
    /* 第7步：返回 */
    sret
