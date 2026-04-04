/**
 * @file hook.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief hooks for scheduler to collect information
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/units.h>

#include <concepts>

namespace schd {
    namespace tags {
        struct tag {};
        struct empty : public tag {};
        struct on_tick : public tag {};
    }  // namespace tags

    class SchdHooks {
    public:
    protected:
        static void on_tick(units::tick gap_ticks)
        {
        }
    };
};  // namespace schd