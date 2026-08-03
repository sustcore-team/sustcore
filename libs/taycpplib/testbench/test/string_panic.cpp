/**
 * @file string_panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::string 非法操作时的 panic 行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/string.h>

#include <cstddef>

template <class T>
struct failing_allocator {
    using value_type = T;

    tay::expected<T *, tay::error_code> try_allocate(size_t) noexcept {
        return tay::expected<T *, tay::error_code>(tay::unexpect, tay::error_code::OUT_OF_MEMORY);
    }

    void deallocate(T *, size_t) noexcept {}
};

int main() {
    tay::string<failing_allocator<char>> text("this constructor must allocate dynamically");
    (void)text;
}
