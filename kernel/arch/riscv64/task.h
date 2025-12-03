/**
 * @file task.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISCV64架构相关进程内容
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <task/task_struct.h>

/**
 * @brief 为进程设置架构相关内容
 *
 * @param p 进程PCB指针
 */
void arch_setup_proc(PCB *p);

/**
 * @brief 为进程设置第argno个参数的值
 *
 * @param p 进程PCB指针
 * @param argno 参数编号 (从0开始)
 * @param value 参数值
 */
void arch_setup_argument(PCB *p, int argno, umb_t value);