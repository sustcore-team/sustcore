/**
 * @file task_struct.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <mem/vma.h>
#include <schd/rr.h>
#include <sus/list.h>

typedef int tid_t;
typedef int pid_t;


// Make sure that TCB is has standard layout,
// so that we can use offsetof to get the TCB pointer from the SU pointer.
class TCB
{
};