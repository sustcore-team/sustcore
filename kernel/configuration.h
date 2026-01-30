/**
 * @file configuration.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构配置
 * @version alpha-1.0.0
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <arch/trait.h>
#include <mem/gfp.h>
#include <mem/alloc.h>
#include <mem/buddy.h>

// Riscv 64

#include <arch/riscv64/trait.h>
#include <arch/riscv64/configuration.h>

// using GFP = LinearGrowGFP;
using GFP = BuddyAllocator;
using Allocator = LinearGrowAllocator;
using PageMan = ArchPageMan<GFP>;
using Serial = ArchSerial;
using Initialization = ArchInitialization;
using MemLayout = ArchMemoryLayout;
using Interrupt = ArchInterrupt;
using Context = ArchContext;
using WPFault = ArchWPFault;