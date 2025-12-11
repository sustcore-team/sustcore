/**
 * @file task_struct.h
 * @author theflysong (song_of_the_fly@163.com), jeromeyao
 * (yaoshengqi726@outlook.com)
 * @brief 进程相关数据结构定义
 * @version alpha-1.0.0
 * @date 2025-12-2
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>
#include <mem/vmm.h>
#include <sus/bits.h>
#include <sus/ctx.h>
#include <sus/list_helper.h>

typedef int tid_t;
typedef tid_t pid_t;

// 线程状态
typedef enum {
    TS_EMPTY     = 0,
    TS_READY     = 1,
    TS_RUNNING   = 2,
    TS_BLOCKED   = 3,
    TS_SUSPENDED = 4,
    TS_ZOMBIE    = 5,
    TS_UNUSED    = 6,
    TS_YIELD     = 7
} ThreadState;

typedef struct TCBStruct {
    // 形成链表结构
    struct TCBStruct *prev;
    struct TCBStruct *next;
    // 所属进程控制块
    struct PCBStruct *pcb;
    // 调度队列
    struct TCBStruct *sprev;
    struct TCBStruct *snext;
    // tid
    tid_t tid;
    // 内核栈指针
    void *kstack;
    // 线程入口点
    void *entrypoint;
    /**
     * @brief 线程优先级
     *
     * 进程中的线程采取优先级调度.
     * 优先调度优先级高(数值低)的线程.
     *
     */
    int priority;
    /**
     * @brief 线程状态
     *
     */
    ThreadState state;
    /**
     * @brief 线程上下文
     *
     */
    RegCtx *ctx;
    /**
     * @brief 线程指令指针
     *
     */
    void **ip;
    /**
     * @brief 线程栈指针
     *
     */
    void **sp;
} TCB;

typedef struct PCBStruct {
    // 形成链表结构
    struct PCBStruct *prev;
    struct PCBStruct *next;

    // 形成链表结构(调度链表)
    struct PCBStruct *sprev;
    struct PCBStruct *snext;

    // 形成进程树结构
    struct PCBStruct *parent;
    struct PCBStruct *children_head;
    struct PCBStruct *children_tail;
    struct PCBStruct *sibling_prev;
    struct PCBStruct *sibling_next;

    // PID
    pid_t pid;
    // 进程状态
    // 进程状态与线程状态没有关系
    // 对线程而言, 其总认为进程是RUNNING状态
    ThreadState state;
    // 进程内存信息
    TM *tm;
    // 线程栈基址
    void *thread_stack_base;

    // 进程优先级
    int rp_level;

    // RP: Ready Process
    // 调度采取四级就绪队列, 四级序列有不同的调度策略
    // rp0: 实时级调度, 只有极少数设备驱动进程会进入该队列.
    // 采用先来先服务(FCFS)调度算法. 一旦进入该队列,
    // 进程将一直运行直到完成或阻塞. rp1: 服务进程队列,
    // 采用时间片轮转(RR)调度算法, 时间片较长. 一般用于提供系统服务的进程.
    // 例如vfs进程 rp2: 普通用户进程队列, 采用时间片轮转(RR)调度算法,
    // 时间片较短. 适用于大多数用户进程.
    // (rp2队列中的进程同时另有一套priority机制,
    // 用于在rp2队列内进一步区分进程优先级.
    // 具有高priority的进程拥有更多的CPU时间片.) rp3: Daemon进程队列,
    // 采用最少运行优先(SJF)调度算法. 适用于后台守护进程,
    // 优先调度运行时间较短的进程. 同时, 高级别队列的进程优先调度,
    // 只有当高优先级队列为空时, 才会调度低优先级队列的进程.
    // 并且这样的调度是抢占式的. 即一旦有高优先级进程进入就绪状态,
    // 立即抢占低优先级进程的CPU使用权.

    // rp0 调度信息

    // rp1 调度信息
    // 调用计数
    int called_count;
    // 剩余时间片计数器
    int rp1_count;

    // rp2 调度信息
    // 优先级
    int priority;
    // 剩余时间片计数器
    int rp2_count;

    // rp3 调度信息
    // 进程运行时间统计(只有Daemon队列使用) (ms)
    int run_time;

    // 进程持有的线程链表(头尾)
    TCB *threads_head;
    TCB *threads_tail;

    // 当前运行线程
    TCB *current_thread;

    // 就绪队列中的线程链表(头尾)
    // 我们首先调度进程, 再调度进程中的线程
    TCB *ready_threads_head;
    TCB *ready_threads_tail;

    // 主线程(进程创建时创建的第一个线程)
    TCB *main_thread;

    // Capabilities
    CSpace *cap_spaces;

    // 持有的能力链表
    Capability *all_cap_head;
    Capability *all_cap_tail;
} PCB;

#define CHILDREN_TASK_LIST(task) \
    task->children_head, task->children_tail, sibling_next, sibling_prev

#define CAPABILITY_LIST(task) task->all_cap_head, task->all_cap_tail, next, prev

#define THREAD_LIST(task) task->threads_head, task->threads_tail, next, prev

#define READY_THREAD_LIST(task)                                       \
    task->ready_threads_head, task->ready_threads_tail, snext, sprev, \
        priority, ascending
