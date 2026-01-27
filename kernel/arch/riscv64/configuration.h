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

using ArchSerial = Riscv64Serial;
using ArchInitialization = Riscv64Initialization;
using ArchMemoryLayout = Riscv64MemoryLayout;
using ArchContext = Riscv64Context;
using ArchInterrupt = Riscv64Interrupt;

template<PageFrameAllocatorTrait PFA>
using ArchPageMan = Riscv64SV39PageMan<PFA>;