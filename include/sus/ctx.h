/**
 * @file ctx.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/arch.h>

struct TCBStruct;
typedef struct TCBStruct TCB;
struct PCBStruct;
typedef struct PCBStruct PCB;

#if __ARCHITECTURE__ == riscv64
#include <arch/riscv64/ctx.h>
#include <arch/riscv64/task.h>
#endif