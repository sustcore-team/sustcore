/**
 * @file set.cpp
 * @brief Demonstrate unique-key storage with tay::set.
 * @version 0.1.0-dev.1
 * @date 2026-07-29
 */

#include <tay/set.h>

#include <cstdio>

namespace {
    using id_set = tay::hash_set<int>;

    template <class Result>
    bool succeeded(const Result& result, const char* operation) {
        if (result) {
            return true;
        }
        std::printf("%s failed with error code %u\n", operation,
                    static_cast<unsigned>(result.error()));
        return false;
    }

    void print(const char* label, const id_set& ids) {
        std::printf("%-12s {", label);
        bool first = true;
        for (int id : ids) {
            std::printf("%s%d", first ? "" : ", ", id);
            first = false;
        }
        std::printf("}\n");
    }
}  // namespace

int main() {
    id_set active_ids{7, 11, 7, 23};
    print("created:", active_ids);
    std::printf("unique count: %zu\n", active_ids.size());

    auto inserted  = active_ids.insert(42);
    auto duplicate = active_ids.insert(11);
    if (!succeeded(inserted, "insert(42)") ||
        !succeeded(duplicate, "insert(11)"))
    {
        return 1;
    }
    std::printf("inserted 42: %s\n", inserted->second ? "yes" : "no");
    std::printf("inserted duplicate 11: %s\n",
                duplicate->second ? "yes" : "no");

    std::printf("contains 23: %s\n", active_ids.contains(23) ? "yes" : "no");
    std::printf("contains 99: %s\n", active_ids.contains(99) ? "yes" : "no");

    active_ids.erase(7);
    if (!succeeded(active_ids.max_load_percent(80), "max_load_percent") ||
        !succeeded(active_ids.rehash(8), "rehash"))
    {
        return 1;
    }
    print("edited:", active_ids);
    std::printf("max load: %zu%%, buckets: %zu\n",
                active_ids.max_load_percent(), active_ids.bucket_count());
    return 0;
}
