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

typedef int tid_t;
typedef int pid_t;

enum class ThreadState { EMPTY = 0, READY = 1, RUNNING = 2, YIELD = 3 };

constexpr const char *to_string(ThreadState state) {
    switch (state) {
        case ThreadState::EMPTY:   return "EMPTY";
        case ThreadState::READY:   return "READY";
        case ThreadState::RUNNING: return "RUNNING";
        case ThreadState::YIELD:   return "YIELD";
        default:                   return "UNKNOWN";
    }
}

struct PCB;

struct TCB {
    // 总线程链表
    TCB *next, *prev;
    // 进程线程链表
    TCB *process_next, *process_prev;
    // 调度队列
    TCB *schedule_next, *schedule_prev;

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
    union Schedule {
        struct {
        } rp0;
        struct {
            int priority;
            int count;
        } rp1;
        struct {
            int priority;
            int count;
        } rp2;
        struct {
            int runtime;
        } rp3;
    } schedule;
    TCB(tid_t tid, PCB *pcb, Runtime runtime, Schedule schedule);
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
    using ThreadList =
        util::IntrusiveList<TCB, &TCB::process_next, &TCB::process_prev>;
    ThreadList threads;
    TCB *main_thread;

    PCB(pid_t pid, int rp_level, TCB *main_thread);
    PCB();
};