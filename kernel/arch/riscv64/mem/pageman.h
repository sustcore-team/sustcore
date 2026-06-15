/**
 * @file pageman.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页表管理器
 * @version alpha-1.0.0
 * @date 2026-06-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <arch/riscv64/mem/sv39.h>

namespace rv64
{
    using PageMan = SV39PageMan;
}