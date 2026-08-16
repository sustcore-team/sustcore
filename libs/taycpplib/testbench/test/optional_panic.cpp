/**
 * @file optional_panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::optional 空值访问的 panic 行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#include <tay/optional.h>

int main() {
    tay::optional<int> empty;
    return empty.value();
}
