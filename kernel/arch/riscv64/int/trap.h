/**
 * @file trap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ISR进入/退出例程
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <arch/riscv64/csr.h>
#include <arch/riscv64/trait.h>
#include <arch/riscv64/configuration.h>
#include <sus/types.h>

/**
 * @brief ISR入口处理宏
 *
 * @param name 函数名
 * @param scause scause寄存器变量
 * @param sepc sepc寄存器变量
 * @param stval stval寄存器变量
 * @param ctx 指向保存的寄存器列表指针
 * @param varsize 为ISR预留的栈空间大小(实际上, 不需要预留太大的栈空间,
 * 因为isr一般是作为执行handler的一个跳板)
 *
 * 保存所有通用寄存器
 * 应放于ISR入口处
 */
#define ISR_ENTRY(name, scause, spec, stval, ctx, varsize) \
    asm volatile ( \
    /* 第1步：切换栈空间 */\
    "csrrw sp, sscratch, sp\n"\
    /* 如果sp为零，说明之前是S-Mode, 否则是U-Mode, 进入不同情况对栈寄存器的处理 */\
    "bne sp, zero, "#name"_entry_user_stkproc\n"\
#name"_entry_kernel_stkproc:\n" \
    /* 第2.1步: 内核态对栈寄存器的处理 */\
    /* 从sscratch中恢复sp（即之前的内核栈） */\
    "csrrw sp, sscratch, sp\n"\
    /* 为了与user_trap保持一致, 先分配栈空间并保存一部分寄存器(x1, x2) */\
    "addi sp, sp, -264\n"\
    "sd x1,  0(sp)\n"\
    /* 我们约定x2是进入isr前的x2(sp), 于是可以先利用已经被保存的x1 */\
    "addi x1, sp, 264\n"\
    "sd x1,  8(sp)\n"\
    /* 进入剩余寄存器的保存 */\
    "j "#name"_entry_rest_context_save\n"\
#name"_entry_user_stkproc:\n"\
    /* 第2.2步: 用户态对栈寄存器的处理 */\
    /* 如果之前在U-Mode，则清空sscratch, 说明已经处于S-Mode */\
    /* 在清空前, 把其值(在经过交换后, 此时sscratch是U-Mode下的sp, x2为S-Mode下的sp)保存到栈上 */\
    /* 于是, 无论如何 我们先分配栈空间保存一部分寄存器(x1, x2) */\
    "addi sp, sp, -264\n"\
    "sd x1,  0(sp)\n"\
    /* 我们约定x2是进入isr前的x2(sp), 我们利用x1读取sscratch并保存sp */\
    "csrr x1, sscratch\n"\
    "sd x1,  8(sp)\n"\
    /* 清空sscratch */\
    "csrw sscratch, x0\n"\
    /* 进入剩余寄存器的保存 */\
#name"_entry_rest_context_save:\n"\
    /* 第3步: 保存剩下的通用寄存器 */\
    "sd x3, 16(sp)\n"\
    "sd x4, 24(sp)\n"\
    "sd x5, 32(sp)\n"\
    "sd x6, 40(sp)\n"\
    "sd x7, 48(sp)\n"\
    "sd x8, 56(sp)\n"\
    "sd x9, 64(sp)\n"\
    "sd x10, 72(sp)\n"\
    "sd x11, 80(sp)\n"\
    "sd x12, 88(sp)\n"\
    "sd x13, 96(sp)\n"\
    "sd x14, 104(sp)\n"\
    "sd x15, 112(sp)\n"\
    "sd x16, 120(sp)\n"\
    "sd x17, 128(sp)\n"\
    "sd x18, 136(sp)\n"\
    "sd x19, 144(sp)\n"\
    "sd x20, 152(sp)\n"\
    "sd x21, 160(sp)\n"\
    "sd x22, 168(sp)\n"\
    "sd x23, 176(sp)\n"\
    "sd x24, 184(sp)\n"\
    "sd x25, 192(sp)\n"\
    "sd x26, 200(sp)\n"\
    "sd x27, 208(sp)\n"\
    "sd x28, 216(sp)\n"\
    "sd x29, 224(sp)\n"\
    "sd x30, 232(sp)\n"\
    "sd x31, 240(sp)\n"\
    /* 第4步: 保存关键CSR寄存器*/\
    "csrr t0, sepc\n" /* spec */\
    "sd t0, 248(sp)\n"\
    "csrr t0, sstatus\n" /* sstatus */\
    "sd t0, 256(sp)\n"\
    /* 第5步: 传参 */\
    "csrr %0, scause\n"\
    "csrr %1, sepc\n"\
    "csrr %2, stval\n"\
    "mv   %3, sp\n"\
    /* 为ISR预留栈空间 */ \
    "addi sp, sp, -"#varsize"\n" \
         : "=r"(scause), "=r"(sepc), "=r"(stval), "=r"(ctx) : : "memory")

/**
 * @brief ISR出口处理宏
 *
 * @param name 函数名
 * @param ctx 指向保存的寄存器列表指针
 *
 * 恢复所有通用寄存器
 * 应放于ISR出口处
 */
#define ISR_EXIT(name, ctx) \
    asm volatile( \
    /* 第0步：恢复sp */\
    "mv   sp, %0\n"\
    /* 第1步：恢复关键CSR寄存器 */\
    "ld t0, 256(sp)\n"             /* 恢复 sstatus */\
    "csrw sstatus, t0\n"\
    "ld t0, 248(sp)\n"             /* 恢复 sepc */\
    "csrw sepc, t0\n"\
    /* 第2步: 对sscratch的特殊处理 */\
    /* 对isr的要求是, 在进入退出程序前, 判断是否是从S-Mode进入的还是从U-Mode进入的, 并据此修改栈上保存的sp值 */\
    /* 我们将把sp值直接保存到sscratch上. 因此判断是从S-Mode进入时, 应将sp值归零 */\
    "ld x1, 8(sp)\n"\
    "csrw sscratch, x1\n"\
    /* 第3步：恢复所有通用寄存器 */\
    "ld x1, 0(sp)\n"\
    /* 其中x2即为sp, 经过了上面的特殊处理 */\
    "ld x3, 16(sp)\n"\
    "ld x4, 24(sp)\n"\
    "ld x5, 32(sp)\n"\
    "ld x6, 40(sp)\n"\
    "ld x7, 48(sp)\n"\
    "ld x8, 56(sp)\n"\
    "ld x9, 64(sp)\n"\
    "ld x10, 72(sp)\n"\
    "ld x11, 80(sp)\n"\
    "ld x12, 88(sp)\n"\
    "ld x13, 96(sp)\n"\
    "ld x14, 104(sp)\n"\
    "ld x15, 112(sp)\n"\
    "ld x16, 120(sp)\n"\
    "ld x17, 128(sp)\n"\
    "ld x18, 136(sp)\n"\
    "ld x19, 144(sp)\n"\
    "ld x20, 152(sp)\n"\
    "ld x21, 160(sp)\n"\
    "ld x22, 168(sp)\n"\
    "ld x23, 176(sp)\n"\
    "ld x24, 184(sp)\n"\
    "ld x25, 192(sp)\n"\
    "ld x26, 200(sp)\n"\
    "ld x27, 208(sp)\n"\
    "ld x28, 216(sp)\n"\
    "ld x29, 224(sp)\n"\
    "ld x30, 232(sp)\n"\
    "ld x31, 240(sp)\n"\
    /* 第4步: 堆栈平衡. */\
    "addi sp, sp, 264\n"\
    /* 第5步：最后从sscratch中恢复栈指针(sp). 此时sp指向S-Mode堆栈, sscratch指向U-Mode堆栈. 交换即可 */\
    "csrrw sp, sscratch, sp\n"\
    /* 第6步：梅开二度. 如果sp此时的sp为空, 说明是从S-Mode进入的ISR.  */\
    "bne sp, zero, "#name"_exit\n"\
    /* 再次交换, 将sp换回 */ \
    "csrrw sp, sscratch, sp\n"\
#name"_exit:\n"\
    /* 第7步：返回 */\
    "sret\n"\
    : : "r"(ctx) : "memory");

/**
 * @brief ISR服务函数属性
 *
 * NAKED: 不生成函数前后缀代码
 * ALIGNED(4): 对齐到4字节
 */
#define ISR_SERVICE_ATTRIBUTE NAKED ALIGNED(4)

/**
 * @brief ISR服务函数开始宏
 *
 * @param name 函数名
 * @param varsize 为ISR预留的栈空间大小(实际上, 不需要预留太大的栈空间,
 * 因为isr一般是作为执行handler的一个跳板)
 */
#define ISR_SERVICE_START(name, varsize)       \
    csr_scause_t scause;                       \
    umb_t sepc, stval;                         \
    ArchContext *ctx; \
    ISR_ENTRY(name, scause.value, sepc, stval, ctx, varsize)

/**
 * @brief ISR服务函数结束宏
 *
 * @param name 函数名
 */
#define ISR_SERVICE_END(name)             \
    do {                                  \
        if ((ctx->sstatus.spp)) { \
            ctx->regs[1] = 0;     \
        }                                 \
        ISR_EXIT(name, ctx);      \
    } while (0)
