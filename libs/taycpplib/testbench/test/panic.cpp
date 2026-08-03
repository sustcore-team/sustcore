/**
 * @file panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay panic 入口的错误报告和终止行为。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/panic.h>

int main() {
    tay::panic("tay panic test");
}
