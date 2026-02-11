/**
 * @file hooks.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 
 * @version alpha-1.0.0
 * @date 2026-02-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <schd/hooks.h>
#include <event/event.h>
#include <task/task_struct.h>

namespace schd {
    void TimerTickListener::handle(const TimerTickEvent &event)
    {
        if (scheduler != nullptr) {
            TCB *current_thread = scheduler->current();
            // 信息收集
            if (current_thread != nullptr) {
                schd::hooks::on_tick(current_thread, event.gap_ticks);
            }

            // 调用调度方法
        }
    }
}