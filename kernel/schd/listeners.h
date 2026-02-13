/**
 * @file listeners.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 监听器
 * @version alpha-1.0.0
 * @date 2026-02-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <event/misc_events.h>

namespace schd {
    class SchedulerListener {
    public:
        static void handle(SchedulerEvent &event);
        static void handle(TimerTickEvent &event);
    };
}