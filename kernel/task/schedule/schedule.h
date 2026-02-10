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

#include <concepts>
#include <type_traits>

// forward declaration of PCB and TCB
class PCB;
class TCB;

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