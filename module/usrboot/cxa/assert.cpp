/**
 * @file assert.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核运行时断言诊断与终止处理
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <log.h>

namespace std {
    [[noreturn]] void __stdlib_assert_fail(const char *file, int line, const char *function,
                                           const char *condition) {
        logger::panic("断言失败: {}, 位置 {}:{} ({})", condition, file, line, function);
    }
}  // namespace std
