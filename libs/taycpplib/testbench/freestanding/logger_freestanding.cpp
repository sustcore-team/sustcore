/**
 * @file logger_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 日志接口可在 freestanding 环境中编译和使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/logger.h>

#include <cstddef>

namespace {
    char output[256]{};
    size_t output_size = 0;

    struct log_output {
        int operator()(const char* data, size_t size) {
            for (size_t index = 0; index < size; ++index) {
                output[output_size++] = data[index];
            }
            return static_cast<int>(size);
        }
    };

    using loggers = tay::logger<log_output>;
}  // namespace

int main() {
    loggers logger;
    auto debug = logger.debug("boot={}", true);
    auto error = logger.error("cause={:x}", 42u);
    return debug && error && output_size != 0 ? 0 : 1;
}
