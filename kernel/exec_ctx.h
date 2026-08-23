/**
 * @file exec_ctx.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief execution context 相关断言
 * @version 0.1.0-dev.1
 * @date 2026-08-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cpu/local.h>

namespace kernel {
    /**
     * @brief 断言当前路径不是硬中断分发。
     * @note 该断言刻意不要求 IRQ 已开启：调度器等普通任务路径会短暂关闭本地 IRQ，再进入
     *       Buddy/SLUB。硬中断则不得分配或修改全局物理页所有权。
     */
    inline void assert_task_ctx() noexcept {
#ifndef NDEBUG
        if (cpu::in_irq())
            __builtin_trap();
#endif
    }
}  // namespace kernel
