/**
 * @file metadata.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 调度元数据
 * @version alpha-1.0.0
 * @date 2026-02-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>
#include <cstddef>

enum class ThreadState {
    EMPTY   = 0,
    READY   = 1,
    RUNNING = 2,
    YIELD   = 3,
    WAITING = 4
};

constexpr const char *to_string(ThreadState state) {
    switch (state) {
        case ThreadState::EMPTY:   return "EMPTY";
        case ThreadState::READY:   return "READY";
        case ThreadState::RUNNING: return "RUNNING";
        case ThreadState::YIELD:   return "YIELD";
        case ThreadState::WAITING: return "WAITING";
        default:                   return "UNKNOWN";
    }
}

namespace schd {
    namespace tags {
        struct tag {};
        struct on_tick : public tag {};
    };

    class FCFSData {
    public:
        using Tags = tags::tag;
        ThreadState state                       = ThreadState::EMPTY;
        util::ListHead<FCFSData> _schedule_head = {};
        FCFSData()                              = default;
        ~FCFSData()                             = default;
    };

    class RRData {
    public:
        using Tags = tags::on_tick;
        ThreadState state                     = ThreadState::EMPTY;
        util::ListHead<RRData> _schedule_head = {};
        size_t _cnt                           = 0;
        RRData()                              = default;
        ~RRData()                             = default;

        inline void on_tick(size_t gap_ticks) {
            if (state == ThreadState::RUNNING && _cnt > 0) {
                --_cnt;
            }
        }
    };
}  // namespace schd