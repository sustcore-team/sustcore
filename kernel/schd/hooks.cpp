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
#include <schd/schedule.h>
#include <task/task_struct.h>

namespace schd {
    void SchedulerListener::handle(TimerTickEvent &event)
    {
        Scheduler *scheduler = Scheduler::get_instance();
        if (scheduler == nullptr) {
            return;
        }
        TCB *current_thread = scheduler->current();
        // 信息收集
        if (current_thread != nullptr) {
            schd::hooks::on_tick(current_thread, event.gap_ticks);
        }
    }

    void SchedulerListener::handle(SchedulerEvent &event)
    {
        event.ret_ctx = do_schedule(event.ctx);
    }
}