/**
 * @file flat_containers.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::flat_set 和 tay::flat_map 的有序连续存储。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/flat.h>

#include <cstdio>

int main() {
    tay::flat_set<int> ids;
    static_cast<void>(ids.insert(30));
    static_cast<void>(ids.insert(10));
    static_cast<void>(ids.insert(20));
    static_cast<void>(ids.insert(20));

    std::printf("flat_set: {");
    bool first = true;
    for (int id : ids) {
        std::printf("%s%d", first ? "" : ", ", id);
        first = false;
    }
    std::printf("}\n");

    tay::flat_map<int, const char *> names;
    static_cast<void>(names.try_emplace(2, "two"));
    static_cast<void>(names.try_emplace(1, "one"));
    static_cast<void>(names.insert_or_assign(2, "TWO"));

    std::printf("flat_map:\n");
    for (auto entry : names) {
        std::printf("  %d -> %s\n", entry.first, entry.second);
    }
    return 0;
}
