/**
 * @file configuration.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 配置
 * @version alpha-1.0.0
 * @date 2026-01-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/trait.h>
#include <arch/trait.h>

using Serial         = Riscv64Serial;
using Initialization = Riscv64Initialization;
using MemoryLayout   = Riscv64MemoryLayout;
using Context        = Riscv64Context;
using Interrupt      = Riscv64Interrupt;
using WPFault        = Riscv64WPFault;
using PageMan        = Riscv64SV39PageMan;