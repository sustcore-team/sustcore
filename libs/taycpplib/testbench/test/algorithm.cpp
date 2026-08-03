/**
 * @file algorithm.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 基础算法的结果和边界行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/algobase.h>

int main() {
    static_assert(tay::min(2, 3) == 2);
    static_assert(tay::max(2, 3) == 3);
    static_assert(tay::abs(-4) == 4);
    static_assert(tay::clamp(8, 1, 5) == 5);

    return 0;
}
