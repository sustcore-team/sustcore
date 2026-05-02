/**
 * @file task.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程与线程
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <exe/task.h>
#include <mem/slub.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sus/nonnull.h>
#include <task/task_struct.h>

class TaskManager
{
private:
    size_t __tid_alloc = 1;
    size_t __pid_alloc = 1;

    size_t alloc_tid() {
        return __tid_alloc++;
    }

    size_t alloc_pid() {
        return __pid_alloc++;
    }

    slub::SlubAllocator<TCB> tcb_pool;
    slub::SlubAllocator<PCB> pcb_pool;

    util::nonnull<TCB *> alloc_tcb() {
        return util::nnullforce(tcb_pool.alloc());
    }

    util::nonnull<PCB *> alloc_pcb() {
        return util::nnullforce(pcb_pool.alloc());
    }

    Result<void> init_tcb(util::nonnull<TCB *> tcb, util::nonnull<PCB *> task/* ... args*/);
    Result<void> init_ctx(util::nonnull<TCB *> tcb, void *entrypoint, void *stack_top);
    Result<void> init_pcb(util::nonnull<PCB *> pcb, TaskSpec spec/* ... args*/);

    Result<void> construct_thread(util::nonnull<PCB *> pcb, void *entrypoint, void *stack_top, schd::ClassType schd_class);
    Result<void> construct_main_thread(util::nonnull<PCB *> pcb, schd::ClassType schd_class);

    Result<void> terminate_tcb(util::nonnull<TCB *> tcb);
    Result<void> terminate_pcb(util::nonnull<PCB *> pcb);

    Result<void> preload(const char *path, TaskSpec &spec, LoadPrm &prm);
public:
    Result<util::nonnull<PCB *>> create_init_task(TaskSpec spec/* ... args*/);
    Result<util::nonnull<PCB *>> create_task(TaskSpec spec, schd::ClassType schd_class/* ... args*/);

    Result<util::nonnull<PCB *>> load_elf(const char *path, schd::ClassType schd_class);
    Result<util::nonnull<PCB *>> load_init(const char *path);
};