/**
 * @file guard.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::guard 的作用域清理和提交操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/guard.h>

#include <cstdio>

int main() {
    bool locked = true;
    {
        tay::guard unlock{[&] {
            locked = false;
            std::puts("automatic cleanup: unlocked");
        }};

        std::puts("working while locked");
    }

    tay::guard rollback{[] { std::puts("rollback"); }};
    rollback.release();
    std::puts(locked ? "still locked" : "cleanup completed");
    return 0;
}
