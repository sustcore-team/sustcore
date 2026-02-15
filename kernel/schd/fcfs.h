/**
 * @file fcfs.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief First Come Fisrt Serve Schedule Algorithm
 * @version alpha-1.0.0
 * @date 2026-02-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <schd/metadata.h>
#include <schd/schedule.h>
#include <sus/list.h>

// Schedule Policies
namespace schd {
    template <typename TCBType>
    class FCFS : public BaseScheduler<TCBType, FCFSData> {
    public:
        using MetadataType = FCFSData;
        using Base         = BaseScheduler<TCBType, FCFSData>;

    protected:
        util::IntrusiveList<MetadataType, &MetadataType::_schedule_head>
            _ready_queue;
        TCBType *_current = nullptr;
        constexpr static bool is_ready(const MetadataType &thread) {
            return thread.state == ThreadState::READY ||
                   thread.state == ThreadState::RUNNING;
        }
        constexpr static bool replacable(const MetadataType &thread) {
            return thread.state == ThreadState::YIELD;
        }
        constexpr static bool runnable(const MetadataType *thread) {
            return thread != nullptr && is_ready(*thread);
        }
        inline void _add(MetadataType *thread) {
            thread->state = ThreadState::READY;
            _ready_queue.push_back(*thread);
        }

        FCFS() : Base(), _ready_queue() {}
        ~FCFS() {}

        static FCFS _instance;

        TCBType *_schedule(void) {
            while (!_ready_queue.empty()) {
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

    public:
        static FCFS *get_instance() {
            return &_instance;
        }

        // 全局对象的构造函数并不会被默认触发, 需要我们手动调用
        static void init_instance() {
            _instance = FCFS();
        }

        inline void add(TCBType *thread) {
            if (thread != nullptr) {
                _add(this->downcast(thread));
            }
        }

        inline TCBType *current(void) {
            return _current;
        }

        inline TCBType *schedule(void) {
            _current = _schedule();
            return _current;
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