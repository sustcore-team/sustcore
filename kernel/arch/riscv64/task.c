/**
 * @file task.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISCV64架构相关进程内容
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/task.h>

void arch_setup_proc(PCB *p) {
    p->ip           = (void **)&p->ctx->sepc;
    p->sp           = (void **)&p->ctx->regs[1];  // sp -> x2
    p->ctx->regs[0] = 0;                          // ra设置为0
    p->ctx->sstatus.spp = 0;                      // 代码运行在 U-Mode
    p->ctx->sstatus.spie = 1;                     // 用户进程应该开启中断
}

/**
 * @brief 为进程设置第argno个参数的值
 *
 * @param p 进程PCB指针
 * @param argno 参数编号 (从0开始)
 * @param value 参数值
 */
void arch_setup_argument(PCB *p, int argno, umb_t value)
{
    if (argno < 0 || argno > 7) {
        // RISCV64最多支持8个整数参数传递
        return;
    }
    // 参数通过a0 - a7传递, 分别对应x10 - x17寄存器
    p->ctx->regs[9 + argno] = value;
}