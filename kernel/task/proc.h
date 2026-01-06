/**
 * @file proc.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-24
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/bits.h>
#include <sus/paging.h>
#include <task/task_struct.h>

/**
 * @brief 进程链表
 *
 */
extern PCB *proc_list_head;
extern PCB *proc_list_tail;
// 进程链表操作宏
#define PROC_LIST proc_list_head, proc_list_tail, next, prev

// 线程栈基址
#define THREAD_STACK_BASE 0x80000000

// 进程就绪队列级别
#define RP_LEVELS (4)

/**
 * @brief 当前线程
 *
 */
extern TCB *cur_thread;

// 就绪队列链表
extern TCB *rp_list_heads[RP_LEVELS];
extern TCB *rp_list_tails[RP_LEVELS];

// 就绪队列操作宏
#define RP_LIST(level) rp_list_heads[level], rp_list_tails[level], snext, sprev

// 我们希望rp3队列中的进程按照run_time从小到大排序
#define RP3_LIST \
    rp_list_heads[3], rp_list_tails[3], snext, sprev, run_time, ascending

extern WaitingTCB *waiting_list_head;
extern WaitingTCB *waiting_list_tail;

#define WAITING_LIST waiting_list_head, waiting_list_tail, next, prev

/**
 * @brief 初始化进程池
 */
void proc_init(void);

/**
 * @brief 新建进程
 *
 * @param tm 进程内存信息
 * @param stack 进程栈顶地址
 * @param heap 进程堆起始地址
 * @param entrypoint 进程入口点
 * @param rp_level RP级别
 * @param parent 父进程指针
 * @return PCB* 新进程PCB指针
 */
PCB *new_task(TM *tm, void *stack, void *heap, void *entrypoint, int rp_level,
              PCB *parent);

/**
 * @brief fork进程
 *
 * @param parent 被fork的父进程
 * @return PCB* 新进程PCB指针
 */
PCB *fork_task(PCB *parent);

/**
 * @brief 清理PCB相关资源
 *
 * @param p PCB指针
 */
void terminate_pcb(PCB *p);

/**
 * @brief 创建线程控制块
 *
 * @param proc 进程PCB指针
 * @param entrypoint 线程入口点
 * @param stack 线程栈指针
 * @return TCB* 线程控制块指针
 */
TCB *new_thread(PCB *proc, void *entrypoint, void *stack, int priority);

/**
 * @brief fork线程
 *
 * @param p 进程PCB指针
 * @param parent_thread 父线程TCB指针
 * @return TCB* 新线程TCB指针
 */
TCB *fork_thread(PCB *p, TCB *parent_thread);

/**
 * @brief 清理TCB相关资源
 *
 * @param t TCB指针
 */
void terminate_tcb(TCB *t);

/**
 * @brief 分配线程栈
 *
 * @param proc 进程PCB指针
 * @param size 线程栈大小
 *
 * @return void* 线程栈顶地址
 */
void *alloc_thread_stack(PCB *proc, size_t size);