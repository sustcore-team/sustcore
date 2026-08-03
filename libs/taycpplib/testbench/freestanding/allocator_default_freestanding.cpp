/**
 * @file allocator_default_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 freestanding 环境中默认 tay::allocator 的可用性。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/allocator.h>
#include <tay/array_list.h>
#include <tay/map.h>
#include <tay/set.h>
#include <tay/string.h>

#include <type_traits>
#include <utility>

consteval bool allocator_constexpr_contract() {
    tay::allocator<int> allocator;
    auto memory = allocator.try_allocate(1);
    if (!memory) {
        return false;
    }
    (void)std::construct_at(*memory, 42);
    const bool valid = **memory == 42;
    std::destroy_at(*memory);
    allocator.deallocate(*memory, 1);
    return valid;
}

using map_value = std::pair<const int, int>;
static_assert(allocator_constexpr_contract());
static_assert(std::is_same_v<tay::string<>, tay::string<tay::allocator<char>>>);
static_assert(std::is_same_v<tay::array_list<int>, tay::array_list<int, tay::allocator<int>>>);
static_assert(
    std::is_same_v<tay::hash_set<int>,
                   tay::hash_set<int, std::hash<int>, std::equal_to<int>, tay::allocator<int>>>);
static_assert(std::is_same_v<tay::hash_map<int, int>,
                             tay::hash_map<int, int, std::hash<int>, std::equal_to<int>,
                                           tay::allocator<map_value>>>);
