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
void schedule(RegCtx **ctx, int time_gap);

/**
 * @brief 在中断后执行的操作
 *
 */
void after_interrupt(RegCtx **ctx);

/**
 * @brief 将进程加入到就绪队列
 * 
 * @param p 进程PCB指针
 */
void insert_ready_process(PCB *p);

/**
 * @brief 唤醒进程
 * 
 * @param p 被唤醒的进程PCB指针
 */
void wakeup_process(PCB *p);

/**
 * @brief 唤醒线程
 * 
 * @param t 被唤醒的线程TCB指针
 */
void wakeup_thread(TCB *t);