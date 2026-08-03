/**
 * @file path_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::path 可在 freestanding 环境中编译和使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/path.h>

#include <type_traits>

static_assert(std::is_same_v<typename tay::path<>::allocator_type, tay::allocator<char>>);

void path_contract() {
    auto path = tay::path<>::try_create("/boot/../kernel");
    if (!path) {
        return;
    }
    auto normalized = path->try_normalize();
    auto relative   = path->try_relative_to(*path);
    (void)normalized;
    (void)relative;
}
