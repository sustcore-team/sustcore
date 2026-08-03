/**
 * @file expected_invalid_rvalue_reference.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 作为 freestanding 编译失败用例，验证 expected 非法右值引用的诊断。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/expected.h>

tay::expected<int &&, int> invalid(tay::unexpect, 1);
