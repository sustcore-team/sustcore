/**
 * @file int.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief intterrupt
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>

class InterruptGuard {
private:
    bool entered      = false;
    bool prev_enabled = false;

public:
    InterruptGuard() = default;

    void enter() {
        if (entered) {
            return;
        }
        prev_enabled = Interrupt::enabled();
        Interrupt::cli();
        entered = true;
    }

    ~InterruptGuard() {
        if (entered && prev_enabled) {
            Interrupt::sti();
        }
    }
};