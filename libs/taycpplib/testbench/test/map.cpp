/**
 * @file map.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::map 的查找、插入、删除和迭代语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/map.h>

#include <cassert>
#include <type_traits>

namespace {
    struct collide_hash {
        constexpr size_t operator()(int) const noexcept {
            return 0;
        }
    };
}  // namespace

using value_type = std::pair<const int, int>;
using map_type   = tay::hash_map<int, int, collide_hash>;

static_assert(std::is_same_v<map_type, tay::hash_map<int, int, collide_hash>>);

int main() {
    map_type values;
    assert(values.empty());
    assert(values.max_load_percent() == 100);

    auto first = values.try_emplace(1, 10);
    assert(first && first->second);
    auto duplicate = values.try_emplace(1, 99);
    assert(duplicate && !duplicate->second);
    assert(duplicate->first->second == 10);

    assert(values.insert(value_type{2, 20}));
    auto assigned = values.insert_or_assign(2, 22);
    assert(assigned && !assigned->second);
    assert(values.at(2) && *values.at(2) == 22);
    auto missing = values.at(9);
    assert(!missing && missing.error() == tay::error_code::OUT_OF_RANGE);

    assert(values.contains(1));
    assert(values.count(2) == 1);
    assert(values.bucket(1) == values.bucket(2));
    assert(values.bucket_size(values.bucket(1)) == 2);

    auto invalid_percent = values.max_load_percent(0);
    assert(!invalid_percent);
    assert(invalid_percent.error() == tay::error_code::INVALID_ARGUMENT);
    assert(values.max_load_percent() == 100);
    assert(values.max_load_percent(250));
    assert(values.max_load_percent() == 250);
    assert(values.reserve(100));
    assert(values.bucket_count() >= 40);
    assert(values.rehash(2));
    assert(values.bucket_count() >= 1);
    assert(values.contains(1) && values.contains(2));

    size_t visited = 0;
    for (const auto& entry : values) {
        assert(entry.first == 1 || entry.first == 2);
        ++visited;
    }
    assert(visited == 2);
    assert(values.erase(1) == 1);
    assert(!values.contains(1));

    auto copied = map_type::try_create(values, values.get_allocator());
    assert(copied && copied->contains(2));
    return 0;
}
