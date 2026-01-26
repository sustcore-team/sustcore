/**
 * @file isr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 中断处理例程
 * @version alpha-1.0.0
 * @date 2025-11-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/configuration.h>
#include <arch/riscv64/int/exception.h>
#include <arch/riscv64/int/trap.h>

/**
 * @brief 通用异常处理程序
 *
 * @param scause scause寄存器 存放异常原因
 * @param sepc   sepc寄存器   存放异常发生时的指令地址
 * @param stval  stval寄存器  存放异常相关的地址或信息
 * @param reglist_ptr 指向中断上下文寄存器列表的指针
 */
void general_exception(csr_scause_t scause, umb_t sepc, umb_t stval,
                       ArchContext *ctx);

/**
 * @brief 非法指令处理程序
 *
 * @param scause scause寄存器 存放异常原因
 * @param sepc sepc寄存器   存放异常发生时的指令地址
 * @param stval stval寄存器  存放异常相关的地址或信息
 * @param reglist_ptr 指向中断上下文寄存器列表的指针
 */
void illegal_instruction_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                                 ArchContext *ctx);

/**
 * @brief 页相关异常处理
 *
 * @param scause scause寄存器 存放异常原因
 * @param sepc   sepc寄存器   存放异常发生时的指令地址
 * @param stval  stval寄存器  存放异常相关的地址或信息
 * @param reglist_ptr 指向中断上下文寄存器列表的指针
 */
void paging_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                    ArchContext *ctx);

/**
 * @brief 定时器中断处理程序
 *
 * @param scause scause寄存器 存放中断原因
 * @param sepc   sepc寄存器   存放中断发生时的指令地址
 * @param stval  stval寄存器  存放中断相关的地址或信息
 * @param reglist_ptr 指向中断上下文寄存器列表的指针
 */
void timer_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                   ArchContext *ctx);

/**
 * @brief 初始化计时器
 *
 * @param freq 计时器频率(Hz)
 * @param expected_freq 期望频率(mHz(10^-3 Hz))
 */
void init_timer(umb_t freq, umb_t expected_freq);