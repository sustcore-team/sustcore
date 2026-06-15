/**
 * @file configuration.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 配置
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/trait.h>
#include <arch/trait.h>

using Serial         = Loongarch64Serial;
using Initialization = Loongarch64Initialization;
using MemoryLayout   = Loongarch64MemoryLayout;
using Context        = Loongarch64Context;
using Interrupt      = Loongarch64Interrupt;
using WPFault        = Loongarch64WPFault;
using PageMan        = Loongarch64PageMan;
