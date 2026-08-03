/**
 * @file format_invalid_partial_spec.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 作为 freestanding 编译失败用例，验证 format 不完整格式说明的诊断。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/format.h>

struct custom_value {};

namespace tay {
    template <>
    struct formatter<custom_value> {
        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            auto position = context.begin();
            if (position != context.end() && *position == 'x') {
                ++position;
            }
            return position;
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const custom_value&, FormatContext& context) const {
            return context.out();
        }
    };
}  // namespace tay

int write_output(const char*, size_t);

void invalid_format() {
    (void)tay::format_to(write_output, "{:xy}", custom_value{});
}
