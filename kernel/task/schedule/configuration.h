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

#include <task/schedule/fcfs.h>
#include <task/schedule/rr.h>

using Scheduler = scheduler::RR;
using ThreadBase = Scheduler::ThreadBase;