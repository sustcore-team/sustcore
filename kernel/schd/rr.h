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
#include <schd/schedule.h>
#include <schd/metadata.h>

// Schedule Policies
namespace schd {
    template <typename TCBType>
    class RR : public BaseScheduler<TCBType, RRData> {
    public:
        using MetadataType = RRData;
        using Base         = BaseScheduler<TCBType, RRData>;
        static constexpr size_t RR_QUANTUM = 5;
    protected:
        util::IntrusiveList<MetadataType, &MetadataType::_schedule_head>
            _ready_queue;
        constexpr static bool is_ready(const MetadataType &thread) {
            return thread.state == ThreadState::READY ||
                   thread.state == ThreadState::RUNNING;
        }
        constexpr static bool replacable(const MetadataType &thread) {
            return thread.state == ThreadState::YIELD;
        }
        constexpr static bool runnable(const MetadataType *thread) {
            return thread != nullptr && is_ready(*thread) && thread->_cnt > 0;
        }
        inline void _add(MetadataType *thread) {
            thread->state = ThreadState::READY;
            thread->_cnt = RR_QUANTUM;
            _ready_queue.push_back(*thread);
        }

    public:
        RR() : Base(), _ready_queue() {}
        ~RR() {}
    
        inline void add(TCBType *thread) {
            if (thread != nullptr) {
                _add(this->downcast(thread));
            }
        }

        inline TCBType *current(void) {
            if (_ready_queue.empty()) {
                return nullptr;
            }
            return this->upcast(&_ready_queue.front());
        }

        TCBType *schedule(void) {
            while (! _ready_queue.empty()) {
                // 首先查看就绪队列头部的线程是否可运行
                auto &thread = _ready_queue.front();
                if (runnable(&thread)) {
                    thread.state = ThreadState::RUNNING;
                    return this->upcast(&thread);
                }

                // 否则, 将其从就绪队列中移除
                // 如果可重新加入就绪队列, 则加入就绪队列的尾部
                _ready_queue.pop_front();
                if (replacable(thread)) {
                    _add(&thread);
                }
            }

            return nullptr;
        }

        void yield(TCBType *thread) {
            // 要求thread必须是当前正在运行的线程
            if (thread != this->current()) {
                return;
            }

            if (thread != nullptr) {
                thread->state = ThreadState::YIELD;
            }
        }

        void exit(TCBType *thread) {
            // 要求thread必须是当前正在运行的线程
            if (thread != this->current()) {
                return;
            }

            if (thread != nullptr) {
                thread->state = ThreadState::EMPTY;
            }
        }
    };
}  // namespace schd