/**
 * @file csr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构控制状态寄存器通用抽象
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>

#if defined(__ARCH_RISCV64__)
#include <arch/riscv64/csr.h>
#elif defined(__ARCH_LOONGARCH64__)
#include <arch/loongarch64/csr.h>
#else
#error unsupported architecture
#endif
