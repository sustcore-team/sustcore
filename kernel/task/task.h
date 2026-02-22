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
#include <sus/id.h>
#include <sus/list.h>
#include <task/task_struct.h>
#include <task/listener.h>

extern util::Defer<util::IDManager<>> TID;

class TCBManager {
public:
    static void init();
    static void on_zero_ref(TCB *tcb);
};