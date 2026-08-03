/**
 * @file format_invalid_alternate_spec.cpp
 * @brief 验证不支持的整数 alternate format spec 会在编译期被拒绝。
 */

#include <tay/format.h>

int write_output(const char *, size_t);

void invalid_alternate_spec() {
    (void)tay::format_to(write_output, "{:#d}", 1);
}
