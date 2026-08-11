/**
 * @file cxa_virtual.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief C++ 无效虚函数调用 ABI 处理
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

// Itanium C++ ABI: 未实现/已删除虚函数的终止入口。

extern "C" [[noreturn]] void __cxa_pure_virtual() {
    // kernel::log::panic("调用了纯虚函数");
    while (true);
}

extern "C" [[noreturn]] void __cxa_deleted_virtual() {
    // kernel::log::panic("调用了已删除的虚函数");
    while (true);
}
