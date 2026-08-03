/**
 * @file function_wrappers.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 非拥有和固定存储可调用对象包装器。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/functional.h>

#include <cstdio>

namespace {
    int twice(int value) noexcept {
        return value * 2;
    }

    int invoke(tay::function_ref<int(int) noexcept> callback, int value) {
        return callback(value);
    }
}  // namespace

int main() {
    int offset      = 5;
    auto add_offset = [&offset](int value) noexcept { return value + offset; };

    tay::function_ref<int(int) noexcept> borrowed(add_offset);
    tay::function_ref<int(int) noexcept> function_pointer(twice);

    tay::inplace_function<int(int), 32> owned(add_offset);
    auto copied = owned;

    std::printf("function_ref lambda: %d\n", invoke(borrowed, 7));
    std::printf("function_ref function: %d\n", invoke(function_pointer, 7));
    std::printf("inplace_function copy: %d\n", copied(7));
    return 0;
}
