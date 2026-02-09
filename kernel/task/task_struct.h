/**
 * @file task_struct.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <configuration.h>
#include <sus/list.h>
#include <task/schedule.h>

// 选择调度算法
template <typename SU>
using Scheduler   = scheduler::RR<SU, 10>;
using ThreadState = SUState;

typedef int tid_t;
typedef int pid_t;

struct PCB;
struct TCB : public Scheduler<TCB>::Storage {
    // 总线程链表
    util::ListHead<TCB> __total_head;
    util::ListHead<TCB> __process_head;

    // TID
    tid_t tid;
    PCB *pcb;
    struct Runtime {
        // 上下文
        Context *const ctx;
        // 内核栈
        void *const kstack;
        // 栈顶
        void *const stack_top;
        // 入口点
        void *const entrypoint;
    } runtime;
    TCB(tid_t tid, PCB *pcb, Runtime runtime);
    // 默认Constructor
    TCB();
};

struct PCB {
    // 总进程链表
    PCB *next, *prev;
    // 树结构
    // PCB *parent;
    // PCB *children_head, *children_tail;
    // PCB *sibling_next, *sibling_prev;

    // PID
    pid_t pid;
    // Task Memory
    // TM *tm;
    int rp_level;

    // 线程链表
    using ThreadList = util::IntrusiveList<TCB, &TCB::__process_head>;
    ThreadList threads;
    TCB *main_thread;

    PCB(pid_t pid, int rp_level, TCB *main_thread);
    PCB();
};