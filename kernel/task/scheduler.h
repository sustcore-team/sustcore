/**
 * @file scheduler.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 调度器
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <task/proc.h>

/**
 * @brief 调度器 - 从当前进程切换到下一个就绪进程
 */
void schedule(RegCtx **ctx);

/**
 * @brief 在系统调用后执行的操作
 *
 * @param ctx 上下文指针
 */
void after_syscall(RegCtx **ctx);