/**
 * @file alloc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 分配器
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concept>
#include <cstddef>

template <typename T>
concept AllocatorTrait = requires(size_t size, void* ptr) {
    {
        T::malloc(size)
    } -> std::same_as<void*>;
    {
        T::free(ptr)
    } -> std::same_as<void>;
};