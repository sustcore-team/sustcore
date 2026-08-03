/**
 * @file expected_panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::expected 非法访问时的 panic 行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/expected.h>

int main() {
    tay::expected<int, int> failed = tay::Err(7);
    return failed.value();
}
