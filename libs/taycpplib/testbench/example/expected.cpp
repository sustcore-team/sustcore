/**
 * @file expected.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::expected 的值或错误处理。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/expected.h>
#include <tay/utility.h>

#include <cstdio>

namespace {
    tay::expected<int, const char*> divide(int numerator, int denominator) {
        if (denominator == 0) {
            return tay::Err(static_cast<const char*>("division by zero"));
        }
        return tay::Ok(numerator / denominator);
    }

    void print_result(int numerator, int denominator) {
        auto result = divide(numerator, denominator);

        result.match(tay::overloaded{
            [=](int value) { std::printf("%d / %d = %d\n", numerator, denominator, value); },
            [=](const char* error) {
                std::printf("%d / %d failed: %s\n", numerator, denominator, error);
            },
        });

        const char* state = result.visit(tay::overloaded{
            [](int) { return "value"; },
            [](const char*) { return "error"; },
        });
        std::printf("result state: %s\n", state);
    }
}  // namespace

int main() {
    print_result(12, 3);
    print_result(12, 0);
    return 0;
}
