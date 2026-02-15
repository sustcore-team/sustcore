/**
 * @file policies.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 调度策略
 * @version alpha-1.0.0
 * @date 2026-02-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <schd/fcfs.h>
#include <schd/metadata.h>
#include <schd/rr.h>

namespace schd {
    // doing asserts
    static_assert(SchedulerTrait<RR<RRData>, RRData>,
                  "RR does not satisfy SchedulerTrait");
    static_assert(SchedulerTrait<FCFS<FCFSData>, FCFSData>,
                  "FCFS does not satisfy SchedulerTrait");
}  // namespace schd