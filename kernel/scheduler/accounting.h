/**
 * @file accounting.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 与调度策略分离的实体运行和等待时间记账。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <scheduler/entity.h>

namespace scheduler {
    class SchedAccounting final {
    public:
        void on_enqueue(SchedStatistics &statistics, units::time now) const noexcept {
            statistics.last_enqueue = now;
        }

        void on_dispatch(SchedStatistics &statistics, units::time now) const noexcept {
            if (now >= statistics.last_enqueue)
                statistics.ready_wait = statistics.ready_wait + (now - statistics.last_enqueue);
            statistics.last_start = now;
        }

        [[nodiscard]] units::duration on_stop(SchedStatistics &statistics,
                                              units::time now) const noexcept {
            const auto elapsed =
                now >= statistics.last_start ? now - statistics.last_start : units::duration{};
            statistics.runtime = statistics.runtime + elapsed;
            return elapsed;
        }

        void on_migrate(SchedStatistics &statistics) const noexcept {
            ++statistics.migrations;
        }
    };
}  // namespace scheduler
