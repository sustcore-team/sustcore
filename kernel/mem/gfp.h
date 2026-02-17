/**
 * @file gfp.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Get Free Page
 * @version alpha-1.0.0
 * @date 2026-02-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/addr.h>
#include <mem/buddy.h>
#include <mem/gfp_def.h>

using GFP = BuddyAllocator;