/**
 * @file init_events.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 初始化事件
 * @version alpha-1.0.0
 * @date 2026-02-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/defer.h>

// 第一批全局对象初始化事件
// 该初始化事件发生时, 内存管理器尚未初始化
// 因此禁止使用new/delete等依赖内存分配的操作
struct PreGlobalObjectInitEvent {
public:
    constexpr PreGlobalObjectInitEvent() {}
};

// 第二批全局对象初始化事件
// 该初始化事件发生时, 内存管理器已完全初始化
struct PostGlobalObjectInitEvent {
public:
    constexpr PostGlobalObjectInitEvent() {}
};