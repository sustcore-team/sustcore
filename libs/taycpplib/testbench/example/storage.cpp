/**
 * @file storage.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 容器存储策略的选择和使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/bitmap.h>
#include <tay/fifo.h>
#include <tay/set.h>
#include <tay/slot_map.h>
#include <tay/static_vector.h>

#include <cstdio>

int main() {
    tay::static_vector<int, 8> values{4, 1, 7};
    tay::static_hash_set<int, 8> unique;
    for (int value : values) {
        static_cast<void>(unique.insert(value));
    }

    tay::static_bitmap<16> flags;
    static_cast<void>(flags.set(4));
    static_cast<void>(flags.set(7));

    tay::static_fifo<int, 4> work;
    static_cast<void>(work.push(4));
    static_cast<void>(work.push(7));

    tay::static_slot_map<int, 4> objects;
    auto handle = objects.emplace(42);

    std::printf("values=%zu unique=%zu bits=%zu fifo=%zu object=%d\n", values.size(), unique.size(),
                flags.count(), work.size(), handle ? *objects.get(*handle) : -1);
    return 0;
}
