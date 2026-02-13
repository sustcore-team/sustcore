/**
 * @file schedule.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 调度器
 * @version alpha-1.0.0
 * @date 2026-02-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <schd/schedule.h>
#include <task/task_struct.h>

namespace schd {
    bool schedule_start_flag = false;
    Context *do_schedule(Context *ctx) {
        // 未开始调度
        if (! schedule_start_flag) {
            return nullptr;
        }
        // 获得调度器
        Scheduler *scheduler = Scheduler::get_instance();
        if (scheduler == nullptr) {
            return nullptr;
        }

        // 获得当前线程和下一个线程
        TCB *current_thread = scheduler->current();
        TCB *next_thread    = scheduler->schedule();

        if (next_thread == nullptr) {
            SCHEDULER::ERROR("无可运行线程!");
            // 此时应该通过某种方法调度空线程
            // 即: next_thread = ...
            while (true);
        }

        // 保持当前线程不变
        if (current_thread == next_thread) {
            SCHEDULER::DEBUG("继续运行线程 TID=%d", current_thread->tid);
            return ctx;
        }

        if (current_thread != nullptr) {
            SCHEDULER::DEBUG("切换线程 TID=%d -> TID=%d", current_thread->tid,
                             next_thread->tid);
        } else {
            SCHEDULER::DEBUG("切换线程 NULL -> TID=%d", next_thread->tid);
        }

        bool switch_pgd_flag = current_thread == nullptr ||
                               current_thread->pcb != next_thread->pcb;
        if (switch_pgd_flag) {
            SCHEDULER::DEBUG(
                "切换页表(from PID=%d to PID=%d)",
                current_thread == nullptr ? -1 : current_thread->pcb->pid,
                next_thread->pcb->pid);
            // switch pgd
        }

        return next_thread->runtime.ctx;
    }
}  // namespace schd