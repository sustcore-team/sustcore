/**
 * @file map.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::map 的无异常键值存储。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/map.h>

#include <cstdio>

namespace {
    using status_map = tay::hash_map<int, const char*>;

    template <class Result>
    bool succeeded(const Result& result, const char* operation) {
        if (result) {
            return true;
        }
        std::printf("%s failed with error code %u\n", operation,
                    static_cast<unsigned>(result.error()));
        return false;
    }

    void print(const status_map& statuses) {
        std::printf("statuses:\n");
        for (const auto& [code, text] : statuses) {
            std::printf("  %d -> %s\n", code, text);
        }
    }
}  // namespace

int main() {
    status_map statuses;
    if (!succeeded(statuses.try_emplace(200, "OK"), "try_emplace(200)") ||
        !succeeded(statuses.try_emplace(404, "Not Found"), "try_emplace(404)") ||
        !succeeded(statuses.insert_or_assign(202, "Accepted"), "insert_or_assign(202)"))
    {
        return 1;
    }

    // Duplicate insertion keeps the existing mapped value.
    auto duplicate = statuses.try_emplace(200, "Already Exists");
    if (!succeeded(duplicate, "duplicate try_emplace")) {
        return 1;
    }
    std::printf("inserted duplicate 200: %s\n", duplicate->second ? "yes" : "no");

    if (!succeeded(statuses.max_load_percent(75), "max_load_percent") ||
        !succeeded(statuses.reserve(16), "reserve"))
    {
        return 1;
    }
    std::printf("max load: %zu%%, buckets: %zu\n", statuses.max_load_percent(),
                statuses.bucket_count());
    print(statuses);

    auto found = statuses.at(202);
    if (!succeeded(found, "at(202)")) {
        return 1;
    }
    std::printf("202 means: %s\n", *found);

    auto missing = statuses.at(500);
    if (!missing) {
        std::printf("at(500) returned error code %u\n", static_cast<unsigned>(missing.error()));
    }
    return 0;
}
