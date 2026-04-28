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
#include <cap/cholder.h>
#include <mem/vma.h>
#include <schd/fcfs.h>
#include <schd/rr.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sus/owner.h>
#include <sus/tree.h>
#include <sustcore/addr.h>

using tid_t = size_t;
using pid_t = size_t;

struct TCB;
struct PCB;

// Make sure that TCB is has standard layout,
// so that we can use offsetof to get the TCB pointer from the SU pointer.
// TCB are arranged as a linked list
struct TCB {
    // thread info
    tid_t tid;
    PCB *task;
    util::ListHead<TCB> list_head;

    // running information
    constexpr static size_t KSTACK_PAGES = 4;  // 16KB(4 pages) for kernel stack
    constexpr static size_t KSTACK_SIZE  = KSTACK_PAGES * PAGESIZE;
    void *kstack_top;
    PhyAddr kstack_phy;

    // get the pointer to the context of this thread
    Context *context() {
        return Context::from_kstack(kstack_top);
    }

    //  schedule data
    schd::ClassType schd_class;
    schd::SchedMeta basic_entity;
    schd::rr::Entity rr_entity;
};

// PCB are arranged as a tree
struct PCB : public util::tree_base::TreeBase<PCB> {
    // process info
    pid_t pid;

    // the threads in this process
    util::IntrusiveList<TCB, &TCB::list_head> threads;

    // resources
    util::owner<TaskMemoryManager *> tmm;
    CHolder *cholder;

    // initialization information
    VirAddr entrypoint;
};