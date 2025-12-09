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

#include <sus/bits.h>
#include <sus/ctx.h>
#include <cap/capability.h>
#include <mem/vmm.h>

typedef int pid_t;

static pid_t PIDALLOC = 1;

static inline pid_t get_current_pid() {
    return PIDALLOC++;
}

// 进程状态
typedef enum {
    PS_EMPTY     = 0,
    PS_READY     = 1,
    PS_RUNNING   = 2,
    PS_BLOCKED   = 3,
    PS_SUSPENDED = 4,
    PS_ZOMBIE    = 5,
    PS_UNUSED    = 6,
    PS_YIELD     = 7
} ProcState;

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
    ProcState state;
    // 上下文
    RegCtx *ctx;
    // 内核栈指针
    void *kstack;
    // 进程内存信息
    TM *tm;
    // 进程入口点
    void *entrypoint;
    // 进程ip寄存器
    void **ip;
    // 进程sp寄存器
    void **sp;

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

    // Capabilities
    CSpace *cap_spaces;

    // 持有的能力链表
    Capability *all_cap_head;
    Capability *all_cap_tail;
} PCB;

#define CHILDREN_TASK_LIST(task) \
    task->children_head, task->children_tail, sibling_next, sibling_prev

#define CAPABILITY_LIST(task) task->all_cap_head, task->all_cap_tail, next, prev