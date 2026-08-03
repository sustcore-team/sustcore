/**
 * @file unique_ptr.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::unique_ptr 的对象、数组和 owner 互操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/owner.h>
#include <tay/unique_ptr.h>

#include <cstdio>
#include <utility>

namespace {
    struct record {
        int id;
    };
}  // namespace

int main() {
    auto item = tay::make_unique<record>(record{42});
    std::printf("record id: %d\n", item->id);

    tay::unique_ptr<record> moved{std::move(item)};
    auto owned = moved.release_owner();
    tay::unique_ptr<record> adopted{std::move(owned)};
    std::printf("adopted id: %d\n", adopted->id);

    auto values = tay::make_unique<int[]>(4);
    for (size_t i = 0; i < 4; ++i) {
        values[i] = static_cast<int>(i * i);
    }
    std::printf("array: %d %d %d %d\n", values[0], values[1], values[2], values[3]);
    return 0;
}
