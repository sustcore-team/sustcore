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
#include <sus/units.h>

struct TimerTickEvent {
public:
    units::tick gap_ticks;
    constexpr TimerTickEvent(units::tick gap_ticks) : gap_ticks(gap_ticks) {}
};