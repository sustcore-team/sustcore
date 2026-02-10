/**
 * @file fcfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief First Come First Serve Schedule Algorithm Implementation
 * @version alpha-1.0.0
 * @date 2026-02-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <task/schedule/configuration.h>

namespace scheduler {
    FCFS::ThreadBase::ThreadBase() {}
    FCFS::ThreadBase::~ThreadBase() {}

    FCFS::FCFS() : _ready_queue() {}
    FCFS::~FCFS() {}

    TCB *FCFS::schedule(void) {
        if (_ready_queue.empty()) {
            return nullptr;
        }

        // 如果首个进程仍在运行, 则继续运行
        ThreadBase &first = _ready_queue.front();
        if (runnable(&first)) {
            first.state = ThreadState::RUNNING;
            return upcast(&first);
        }

        // 否则, first移出队列
        _ready_queue.pop_front();
        if (replacable(first)) {
            _add(&first);
        }

        // 寻找下个可运行的进程
        while (! _ready_queue.empty()) {
            ThreadBase &next = _ready_queue.front();
            if (runnable(&next)) {
                next.state = ThreadState::RUNNING;
                return upcast(&next);
            }

            // 否则, 移出队列
            _ready_queue.pop_front();
            if (replacable(next)) {
                _add(&next);
            }
        }

        // 没有可运行的进程
        return nullptr;
    }
}  // namespace scheduler