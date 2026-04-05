/**
 * @file epacks.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Event Packs
 * @version alpha-1.0.0
 * @date 2026-03-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/units.h>
#include <sustcore/addr.h>

// Timer Tick Event Pack
struct TimerTickEvent {
    units::tick last_tick;
    units::tick increment;
    units::tick gap_ticks;
};

// No Present Event Pack
struct NoPresentEvent {
    VirAddr access_address;
};