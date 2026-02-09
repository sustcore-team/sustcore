/**
 * @file schedule.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Scheduler
 * @version alpha-1.0.0
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>

enum class SUState { EMPTY = 0, READY = 1, RUNNING = 2, YIELD = 3 };

constexpr const char *to_string(SUState state) {
    switch (state) {
        case SUState::EMPTY:   return "EMPTY";
        case SUState::READY:   return "READY";
        case SUState::RUNNING: return "RUNNING";
        case SUState::YIELD:   return "YIELD";
        default:               return "UNKNOWN";
    }
}

// Schedule Policies
namespace scheduler {
    template <typename SchduPolicy>
    concept SchedulePolicyTrait =
        requires(SchduPolicy policy) { typename SchduPolicy::Storage; };

    /**
     * @brief 先来先调度
     *
     * @tparam SU Schedule Unit
     */
    template <typename SU>
    class FCFS {
    public:
        class Storage {
        public:
            SUState state;
            Storage() : state(SUState::EMPTY), _schedule_head() {}

        protected:
            util::ListHead<Storage> _schedule_head;
            friend class FCFS;
        };

    protected:
        util::IntrusiveList<Storage, &Storage::_schedule_head> _ready_queue;
        Storage *downcast(SU *unit) {
            return reinterpret_cast<Storage *>(unit);
        }
        SU *upcast(Storage *storage) {
            return reinterpret_cast<SU *>(storage);
        }
        Storage *_current(void) {
            return _ready_queue.empty() ? nullptr : &_ready_queue.front();
        }
        bool ready_state(Storage *storage) {
            return storage->state == SUState::READY ||
                   storage->state == SUState::RUNNING;
        }
        bool yield_state(Storage *storage) {
            return storage->state == SUState::YIELD;
        }
        bool switchable(Storage *storage) {
            return storage != nullptr && ready_state(storage);
        }

    public:
        FCFS() : _ready_queue() {}
        // 向就绪队列中添加一个调度单元
        void add(SU *unit) {
            _ready_queue.push_back(*downcast(unit));
        }
        SU *current() {
            return upcast(_current());
        }
        SU *schedule(void) {
            if (_ready_queue.empty()) {
                return nullptr;
            }
            // FCFS中, 队列中首个进程即是当前进程
            Storage *first = &_ready_queue.front();
            assert(first == _current());
            // 对首个进程进行判定
            if (switchable(first)) {
                first->state = SUState::RUNNING;
                return upcast(first);
            }
            // 否则, first离开队列
            _ready_queue.pop_front();

            // 如果是yield状态, 则将first加入队列末尾
            if (yield_state(first)) {
                first->state = SUState::READY;
                _ready_queue.push_back(*first);
            }

            // 寻找下个可调度的进程
            while (!_ready_queue.empty()) {
                Storage *next = &_ready_queue.front();
                if (switchable(next)) {
                    next->state = SUState::RUNNING;
                    return upcast(next);
                }
                // 否则, 虽然在ready_queue中, 但不可调度, 将其弹出
                _ready_queue.pop_front();
                // 如果是yield状态, 则将next加入队列末尾
                if (yield_state(next)) {
                    next->state = SUState::READY;
                    _ready_queue.push_back(*next);
                }
            }

            // 否则, 没有可调度的进程
            return nullptr;
        }
    };

    /**
     * @brief 时间片轮转调度
     *
     * @tparam SU Schedule Unit
     */
    template<typename SU, size_t TIME_SLICES>
    class RR {
    public:
        class Storage {
        public:
            SUState state;
            Storage() : state(SUState::EMPTY), _cnt(0), _schedule_head() {}
            size_t cnt() const {
                return _cnt;
            }
        protected:
            size_t _cnt;
            util::ListHead<Storage> _schedule_head;
            friend class RR;
        };
    protected:
        util::IntrusiveList<Storage, &Storage::_schedule_head> _ready_queue;
        Storage *downcast(SU *unit) {
            return reinterpret_cast<Storage *>(unit);
        }
        SU *upcast(Storage *storage) {
            return reinterpret_cast<SU *>(storage);
        }
        Storage *_current(void) {
            return _ready_queue.empty() ? nullptr : &_ready_queue.front();
        }
        bool ready_state(Storage *storage) {
            return storage->state == SUState::READY || storage->state == SUState::RUNNING;
        }
        bool replacable_state(Storage *storage) {
            return storage->state == SUState::YIELD || ready_state(storage);
        }
        bool switchable(Storage *storage) {
            return storage != nullptr && ready_state(storage) && storage->cnt() > 0;
        }
    public:
        RR() : _ready_queue() {}
        // 向就绪队列中添加一个调度单元
        void add(SU *unit) {
            _ready_queue.push_back(*downcast(unit));
            unit->state = SUState::READY;
            unit->_cnt = TIME_SLICES;
        }
        SU *current() {
            return upcast(_current());
        }
        SU *schedule(void) {
            if (_ready_queue.empty()) {
                return nullptr;
            }
            // RR中, 队列中首个进程即是当前进程
            Storage *first = &_ready_queue.front();
            assert (first == _current());
            // 对首个进程进行判定
            if (switchable(first)) {
                first->state = SUState::RUNNING;
                first->_cnt --;
                return upcast(first);
            }
            // 否则, first离开队列
            _ready_queue.pop_front();
            
            // 如果是yield状态, 则将first加入队列末尾
            if (replacable_state(first)) {
                first->state = SUState::READY;
                first->_cnt = TIME_SLICES;
                _ready_queue.push_back(*first);
            }

            // 寻找下个可调度的进程
            while (!_ready_queue.empty()) {
                Storage *next = &_ready_queue.front();
                if (switchable(next)) {
                    next->state = SUState::RUNNING;
                    next->_cnt --;
                    return upcast(next);
                }
                // 否则, 虽然在ready_queue中, 但不可调度, 将其弹出
                _ready_queue.pop_front();
                // 如果是yield状态, 则将next加入队列末尾
                if (replacable_state(next)) {
                    next->state = SUState::READY;
                    first->_cnt = TIME_SLICES;
                    _ready_queue.push_back(*next);
                }
            }

            // 否则, 没有可调度的进程
            return nullptr;
        }
    };
}  // namespace scheduler