/**
 * @file misc_events.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 杂项事件
 * @version alpha-1.0.0
 * @date 2026-02-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>

struct TimerTickEvent {
public:
    size_t gap_ticks;
    constexpr TimerTickEvent(size_t gap_ticks) : gap_ticks(gap_ticks) {}
};