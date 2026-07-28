/**
 * @file expected.cpp
 * @brief Demonstrate value-or-error handling with tay::expected.
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 */

#include <cstdio>

#include <tay/expected.h>

namespace {
    tay::expected<int, const char*> divide(int numerator, int denominator) {
        if (denominator == 0) {
            return tay::Err(static_cast<const char*>("division by zero"));
        }
        return tay::Ok(numerator / denominator);
    }

    void print_result(int numerator, int denominator) {
        auto result = divide(numerator, denominator);
        if (result) {
            std::printf("%d / %d = %d\n", numerator, denominator,
                        result.value());
        } else {
            std::printf("%d / %d failed: %s\n", numerator, denominator,
                        result.error());
        }
    }
}  // namespace

int main() {
    print_result(12, 3);
    print_result(12, 0);
    return 0;
}
