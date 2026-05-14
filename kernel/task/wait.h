/**
 * @file wait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief thread waiting reasons
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>
#include <sus/map.h>
#include <sustcore/errcode.h>
#include <task/task_struct.h>

namespace task::wait {
    // 等待队列
    struct WaitQueue {
        // 等待队列对应的等待id
        WaitReasonId id;
        util::IntrusiveList<TCB, &TCB::wait_head> threads;

        // 从wait id中创建一个等待队列
        explicit WaitQueue(WaitReasonId id) : id(id), threads() {}
    };

    // 等待原因管理器, 负责管理所有的等待队列
    class WaitReasonManager {
    private:
        // 编号分配
        WaitReasonId _next_reason = 1;
        // wait_id -> wait_queue
        util::LinkedMap<WaitReasonId, WaitQueue *> _queues;

        // 获取等待队列, 如果不存在则创建一个新的
        Result<WaitQueue *> queue_for_wait(WaitReasonId id);
        // 获取等待队列, 如果不存在则返回错误
        Result<WaitQueue *> queue_if_exists(WaitReasonId id);
    public:
        // 分配一个 wait reason 号
        WaitReasonId alloc_reason();
        // 将当前线程加入等待队列
        Result<void> enqueue(WaitReasonId id, TCB *tcb);
        // 从等待队列中弹出一个线程
        Result<TCB *> pop_one(WaitReasonId id);
        // 从等待队列中唤醒一个线程, 返回被唤醒线程的数量(0或1)
        Result<size_t> wake_one(WaitReasonId id);
        // 从等待队列中唤醒所有线程, 返回被唤醒线程的数量
        Result<size_t> wake_all(WaitReasonId id);

        // 获取等待原因管理器的单例实例
        static WaitReasonManager &inst();
    };

    WaitReasonId alloc_reason();
    Result<void> wait_current(WaitReasonId id);
    Result<size_t> wake_one(WaitReasonId id);
    Result<size_t> wake_all(WaitReasonId id);
}  // namespace task::wait
