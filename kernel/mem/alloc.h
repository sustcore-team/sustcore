/**
 * @file alloc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Allocator
 * @version alpha-1.0.0
 * @date 2026-02-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/alloc_def.h>
#include <mem/slub.h>

using Allocator = LinearGrowAllocator;

template <typename ObjType>
using KOA = SimpleKOA<ObjType, Allocator>;