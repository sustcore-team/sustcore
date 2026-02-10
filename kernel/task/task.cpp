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
#include <sus/id.h>
#include <task/task.h>

Scheduler *scheduler = nullptr;

// task_struct.h

TCB::TCB(tid_t tid, PCB *pcb, Runtime runtime)
    : tid(tid), pcb(pcb), runtime(runtime)
{
}

// Empty Constructor, for intrusive linked list
TCB::TCB()
    : tid(0), pcb(nullptr), runtime({})
{
}

PCB::PCB(pid_t pid, int rp_level, TCB *main_thread)
    : pid(pid), rp_level(rp_level), threads(), main_thread(main_thread)
{
    threads.push_back(*main_thread);
}

// Empty Constructor, for intrusive linked list
PCB::PCB()
    : pid(0), rp_level(0), threads(), main_thread(nullptr)
{
}

// task.h

util::IDManager<> TID;

void TCBManager::init()
{
    // Construct TID
    TID = util::IDManager<>();
}