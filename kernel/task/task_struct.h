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
#include <schd/policies.h>
#include <sus/list.h>

typedef int tid_t;
typedef int pid_t;

struct PCB;

// 调度算法选择
template <typename TCB>
using _Scheduler = schd::RR<TCB>;

struct TCB : public _Scheduler<TCB>::MetadataType {
    using MetadataType = _Scheduler<TCB>::MetadataType;

    // 总线程链表
    util::ListHead<TCB> __total_head   = {nullptr, nullptr};
    util::ListHead<TCB> __process_head = {nullptr, nullptr};
    util::ListHead<TCB> __waitrel_head = {nullptr, nullptr};

    // TID
    tid_t tid;
    PCB *pcb;
    struct Runtime {
        // 上下文
        Context *ctx;
        // 内核栈
        void *kstack;
        // 栈顶
        void *stack_top;
        // 入口点
        void *entrypoint;
    } runtime;

    TCB(tid_t tid, PCB *pcb, Runtime runtime);
    // 默认Constructor
    TCB();
};

using Scheduler = _Scheduler<TCB>;

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