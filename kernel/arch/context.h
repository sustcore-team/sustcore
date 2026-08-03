/**
 * @file context.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 当前目标架构调度上下文的选择入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/context_trait.h>

#if defined(__ARCH_RISCV64__)
#include <arch/riscv64/context.h>
#elif defined(__ARCH_LOONGARCH64__)
#include <arch/loongarch64/context.h>
#else
#error unsupported architecture
#endif
