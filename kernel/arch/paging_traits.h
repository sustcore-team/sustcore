/**
 * @file paging_traits.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 当前目标架构分页策略的选择入口
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/namespace.h>

#if defined(__ARCH_RISCV64__)
#include <arch/riscv64/paging.h>
#elif defined(__ARCH_LOONGARCH64__)
#include <arch/loongarch64/paging.h>
#else
#error unsupported architecture
#endif

static_assert(hal::PageTableTraits<hal::PtOps>);
