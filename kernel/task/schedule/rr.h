/**
 * @file rr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Round Robin Schedule Algorithm
 * @version alpha-1.0.0
 * @date 2026-02-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/list.h>
#include <task/schedule/schedule.h>
#include <cstddef>

// Schedule Policies
namespace scheduler {
    class RR {
    public:
        static constexpr size_t TIME_SLICES = 10;
        struct ThreadBase {
        public:
            ThreadState state = ThreadState::EMPTY;
            ThreadBase();
            virtual ~ThreadBase();

        protected:
            size_t _cnt = 0;
            util::ListHead<ThreadBase> _schedule_head;
            friend class RR;
        };
    protected:
        util::IntrusiveList<ThreadBase, &ThreadBase::_schedule_head>
            _ready_queue;
        // 实际上, TCB不一定会继承RR::ThreadBase,
        // 然而, TCB一定会继承调度器的ThreadBase
        // 因此, 如果这些方法会被调用, 则TCB一定会继承RR::ThreadBase,
        // 因此可以安全地进行类型转换 然而, 要避免编译错误,
        // 因此仍然需要进行reinterpret_cast
        inline TCB *upcast(ThreadBase *thread) {
            return reinterpret_cast<TCB *>(thread);
        }
        // 实际上, TCB不一定会继承RR::ThreadBase,
        // 然而, TCB一定会继承调度器的ThreadBase
        // 因此, 如果这些方法会被调用, 则TCB一定会继承RR::ThreadBase,
        // 因此可以安全地进行类型转换 然而, 要避免编译错误,
        // 因此仍然需要进行reinterpret_cast
        inline ThreadBase *downcast(TCB *tcb) {
            return reinterpret_cast<ThreadBase *>(tcb);
        }

        constexpr static bool is_ready(const ThreadBase &thread) {
            return thread.state == ThreadState::READY ||
                   thread.state == ThreadState::RUNNING;
        }

        constexpr static bool replacable(const ThreadBase &thread) {
            return thread.state == ThreadState::YIELD || is_ready(thread);
        }

        bool runnable(const ThreadBase *thread) {
            return thread != nullptr && is_ready(*thread) && thread->_cnt > 0;
        }
        inline void _add(ThreadBase *thread) {
            thread->state = ThreadState::READY;
            thread->_cnt = TIME_SLICES;  // TODO: 时间片长度
            _ready_queue.push_back(*thread);
        }
    public:
        RR();
        ~RR();
        inline void add(TCB *thread) {
            if (thread != nullptr) {
                _add(downcast(thread));
            }
        }
        inline TCB *current(void) {
            if (_ready_queue.empty()) {
                return nullptr;
            }

            return upcast(&_ready_queue.front());
        }
        TCB *schedule(void);
    };
}  // namespace scheduler