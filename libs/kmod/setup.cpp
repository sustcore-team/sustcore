/**
 * @file setup.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 初始化内核模块的 C++ 运行时环境并转入内核主入口。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

// cpp setup入口点

#include <cstddef>

extern "C" int kmain();

extern "C" void _cpp_setup(const void *stack_start) {
    if (stack_start == nullptr) {
        while (true) {
        }
    }
    int ret = kmain();
    while (true);
}
