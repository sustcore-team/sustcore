/**
 * @file set.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::set 的插入、查找、删除和迭代语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/set.h>

#include <cassert>
#include <type_traits>
#include <utility>

using set_type = tay::hash_set<int>;

static_assert(std::is_same_v<set_type, tay::hash_set<int>>);
static_assert(
    std::is_const_v<std::remove_reference_t<decltype(*std::declval<set_type::iterator>())>>);

int main() {
    set_type values{1, 2, 2, 3};
    assert(values.size() == 3);
    assert(values.contains(1));
    assert(values.count(4) == 0);

    auto inserted = values.insert(4);
    assert(inserted && inserted->second);
    auto duplicate = values.emplace(4);
    assert(duplicate && !duplicate->second);

    assert(values.max_load_percent(75));
    assert(values.reserve(64));
    assert(values.bucket_count() >= 86);
    assert(values.rehash(1));
    assert(values.bucket_count() >= 6);

    auto range = values.equal_range(3);
    assert(range.first != range.second);
    assert(*range.first == 3);
    values.erase(range.first);
    assert(!values.contains(3));

    auto copied = set_type::try_create(values, values.get_allocator());
    assert(copied && copied->size() == values.size());
    values.clear();
    assert(values.empty());
    return 0;
}
