/**
 * @file panic_link_user.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 freestanding 程序对外部 tay::panic 实现的链接。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/expected.h>

extern "C" int main() {
    tay::expected<int, int> failed = tay::Err(1);
    return failed.value();
}
