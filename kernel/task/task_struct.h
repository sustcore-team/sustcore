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

#include <arch/description.h>
#include <schd/policies.h>
#include <sus/list.h>

typedef int tid_t;
typedef int pid_t;

struct PCB;

// 调度算法选择
template <typename TCB>
using _Scheduler = schd::RR<TCB>;

class TCBManager;

class TCB : public _Scheduler<TCB>::MetadataType {
public:
    using MetadataType = _Scheduler<TCB>::MetadataType;

    // 总线程链表
    util::ListHead<TCB> __total_head   = {nullptr, nullptr};
    util::ListHead<TCB> __process_head = {nullptr, nullptr};
    util::ListHead<TCB> __waitrel_head = {nullptr, nullptr};

    // setup infomations
    struct SetupInfo {
        void *stacktop;
        void *entrypoint;
        void *kstacktop;
        bool is_kernel_thread;
    };
    const tid_t tid;
protected:
    PCB *_pcb;
    SetupInfo _setup;
    // reference count
    size_t _ref_count;
public:
    inline Context *context(size_t size) {
        return (Context *)((umb_t)_setup.kstacktop - sizeof(Context));
    }

    inline PCB *pcb(void) const {
        return _pcb;
    }

    inline size_t ref_count(void) const {
        return _ref_count;
    }

    void *stacktop(void) const {
        return _setup.stacktop;
    }

    void *entrypoint(void) const {
        return _setup.entrypoint;
    }

    void *kstacktop(void) const {
        return _setup.kstacktop;
    }

    void retain(void);
    void release(void);

    // constructors
    TCB(tid_t tid, PCB *pcb, SetupInfo *setup);
    // 默认Constructor
    TCB();

    // TCB不允许被复制或移动
    TCB(const TCB &)            = delete;
    TCB &operator=(const TCB &) = delete;
    TCB(TCB &&)                 = delete;
    TCB &operator=(TCB &&)      = delete;

    void *operator new(size_t size);
    void operator delete(void *ptr);
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

    void *operator new(size_t size);
    void operator delete(void *ptr);
};