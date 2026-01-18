/**
 * @file pfa.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理页框分配器
 * @version alpha-1.0.0
 * @date 2026-01-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <stddef.h>

#define PFA_BUDDY 1

// 默认使用Buddy页框分配器
#ifndef PFA_SYSTEM
#define PFA_SYSTEM PFA_BUDDY
#endif

#if PFA_SYSTEM == PFA_BUDDY
#include <mem/buddy.h>

#define pfa_pre_init     buddy_pre_init
#define pfa_post_init    buddy_post_init
#define pfa_alloc_frame  buddy_alloc_frame
#define pfa_alloc_frames buddy_alloc_frames
#define pfa_free_frame   buddy_free_frame
#define pfa_free_frames  buddy_free_frames
#endif

void pfa_pre_init(MemRegion *layout);
void pfa_post_init(void);
void *pfa_alloc_frame(void);
void *pfa_alloc_frames(size_t count);
void pfa_free_frame(void *frame);
void pfa_free_frames(void *frame, size_t count);