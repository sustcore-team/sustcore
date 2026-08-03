/**
 * @file owner.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::owner 的原始指针所有权标注。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/owner.h>

#include <cstdio>

namespace {
    struct record {
        int id;
    };
}  // namespace

int main() {
    tay::owner owned{new record{42}};
    std::printf("owned record id: %d\n", owned->id);

    // tay::owner is an ownership annotation, so destruction remains explicit.
    delete owned.get();
    return 0;
}
