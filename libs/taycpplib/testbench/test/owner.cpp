/**
 * @file owner.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::owner 的所有权标注和互操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/owner.h>

#include <cassert>

int main() {
    int value = 9;
    tay::owner owned{&value};
    assert(owned && *owned == 9 && owned.get() == &value);

    return 0;
}
