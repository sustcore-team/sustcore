/**
 * @file task.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程/线程
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <sus/defer.h>
#include <sus/id.h>
#include <task/task.h>
#include <task/task_struct.h>

namespace kop {
    util::Defer<KOP<PCB>> PCB;
    AutoDefer(PCB);
    util::Defer<KOP<TCB>> TCB;
    AutoDefer(TCB);
}

TCB::TCB(tid_t tid, PCB *pcb, SetupInfo *setup)
    : tid(tid), _pcb(pcb), _setup(setup) {}

// Empty Constructor, for intrusive linked list
TCB::TCB() : tid(0), _pcb(nullptr), _setup({}) {}

void *TCB::operator new(size_t size) {
    assert(size == sizeof(TCB));
    return kop::TCB->alloc();
}

void TCB::operator delete(void *ptr) {
    kop::TCB->free(static_cast<TCB *>(ptr));
}

PCB::PCB(pid_t pid, TCB *main_thread)
    : pid(pid), threads(), main_thread(main_thread) {
    threads.push_back(*main_thread);
}

// Empty Constructor, for intrusive linked list
PCB::PCB() : pid(0), threads(), main_thread(nullptr) {}

void *PCB::operator new(size_t size) {
    assert(size == sizeof(PCB));
    return kop::PCB->alloc();
}

void PCB::operator delete(void *ptr) {
    kop::PCB->free(static_cast<PCB *>(ptr));
}

// task.h

util::Defer<util::IDManager<>> TID;
AutoDefer(TID);

void TCBManager::init() {
}

/**
 * @brief 默认任务
 * 当没有其他任务可运行时, 调度器将运行该任务
 * 该任务仅仅是一个死循环, 不执行任何有意义的操作
 *
 */
void default_task(void) {
    while (true);
}