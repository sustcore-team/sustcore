/**
 * @file configuration.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 调度器配置
 * @version alpha-1.0.0
 * @date 2026-02-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <schd/fcfs.h>
#include <schd/rr.h>

template <typename TCBType>
using _Scheduler = schd::RR<TCBType>;