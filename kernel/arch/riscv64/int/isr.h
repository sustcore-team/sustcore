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

#include <arch/riscv64/trait.h>

namespace Handlers {
    /**
     * @brief 通用异常处理程序
     *
     * @param scause scause寄存器 存放异常原因
     * @param sepc   sepc寄存器   存放异常发生时的指令地址
     * @param stval  stval寄存器  存放异常相关的地址或信息
     * @param reglist_ptr 指向中断上下文寄存器列表的指针
     */
    void exception(csr_scause_t scause, umb_t sepc, umb_t stval,
                           Riscv64Context *ctx);

    /**
     * @brief 非法指令处理程序
     *
     * @param scause scause寄存器 存放异常原因
     * @param sepc sepc寄存器   存放异常发生时的指令地址
     * @param stval stval寄存器  存放异常相关的地址或信息
     * @param reglist_ptr 指向中断上下文寄存器列表的指针
     */
    void illegal_instruction(csr_scause_t scause, umb_t sepc,
                                     umb_t stval, Riscv64Context *ctx);

    /**
     * @brief 页相关异常处理
     *
     * @param scause scause寄存器 存放异常原因
     * @param sepc   sepc寄存器   存放异常发生时的指令地址
     * @param stval  stval寄存器  存放异常相关的地址或信息
     * @param reglist_ptr 指向中断上下文寄存器列表的指针
     */
    void paging_fault(csr_scause_t scause, umb_t sepc, umb_t stval,
                        Riscv64Context *ctx);

    /**
     * @brief 定时器中断处理程序
     *
     * @param scause scause寄存器 存放中断原因
     * @param sepc   sepc寄存器   存放中断发生时的指令地址
     * @param stval  stval寄存器  存放中断相关的地址或信息
     * @param reglist_ptr 指向中断上下文寄存器列表的指针
     */
    void timer(csr_scause_t scause, umb_t sepc, umb_t stval,
                       Riscv64Context *ctx);
}  // namespace Handlers