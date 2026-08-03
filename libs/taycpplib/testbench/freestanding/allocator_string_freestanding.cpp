/**
 * @file allocator_string_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 freestanding 环境中 tay::string 与分配器的协作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/allocator.h>
#include <tay/string.h>

#include <cstddef>
#include <type_traits>

template <class T>
struct freestanding_allocator {
    using value_type = T;

    constexpr tay::expected<T *, tay::error_code> try_allocate(size_t) noexcept {
        return tay::expected<T *, tay::error_code>(tay::unexpect, tay::error_code::OUT_OF_MEMORY);
    }

    constexpr void deallocate(T *, size_t) noexcept {}
};

using test_string = tay::string<freestanding_allocator<char>>;

static_assert(std::is_nothrow_default_constructible_v<freestanding_allocator<char>>);
static_assert(
    std::is_same_v<decltype(tay::allocator_traits<freestanding_allocator<int>>::try_allocate(
                       std::declval<freestanding_allocator<int> &>(), 1)),
                   tay::expected<int *, tay::error_code>>);
static_assert(sizeof(void *) != 8 || sizeof(test_string) == 32);

void freestanding_string_contract() {
    test_string text("tay");
    (void)text.append(" string");
    (void)text.at(20);
    auto failed = test_string::try_create("this string requires dynamic storage");
    (void)failed;
}
