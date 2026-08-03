/**
 * @file format_invalid_mixed_index.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 作为 freestanding 编译失败用例，验证 format 混用索引方式的诊断。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/format.h>

int write_output(const char *, size_t);

void invalid_format() {
    (void)tay::format_to(write_output, "{} {0}", 1);
}
