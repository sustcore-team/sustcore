/**
 * @file frame.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 当前目标架构 TrapFrame 的选择入口。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#if defined(__ARCH_RISCV64__)
#include <arch/riscv64/interrupt/frame.h>
#elif defined(__ARCH_LOONGARCH64__)
#include <arch/loongarch64/interrupt/frame.h>
#else
#error unsupported architecture
#endif
