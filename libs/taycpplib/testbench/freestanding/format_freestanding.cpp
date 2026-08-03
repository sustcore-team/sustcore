/**
 * @file format_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 格式化接口可在 freestanding 环境中编译和使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/format.h>

#include <cstddef>

namespace {
    char output[128]{};
    size_t output_size = 0;

    struct boot_status {
        int code;
        bool ready;
    };

    int write_output(const char* data, size_t size) {
        for (size_t index = 0; index < size; ++index) {
            output[output_size++] = data[index];
        }
        return static_cast<int>(size);
    }
}  // namespace

namespace tay {
    template <>
    struct formatter<boot_status> {
        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const boot_status& status,
                                                FormatContext& context) const {
            context.write("boot_status{code=");
            context.format("{:05d}", status.code);
            context.write(", ready=");
            context.format("{}", status.ready);
            context.put('}');
            return context.out();
        }
    };
}  // namespace tay

int main() {
    int value          = 42;
    boot_status status = {7, true};
    auto result = tay::format_to<8>(write_output, "boot={} hex={:x} ptr={} status={}", true, value,
                                    &value, status);
    char iterator_output[8]{};
    auto iterator_result =
        tay::format_to_iter_s(iterator_output, sizeof(iterator_output), "{}:{}", true, value);
    return result && *result == output_size && iterator_result ? 0 : 1;
}
