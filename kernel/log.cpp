/**
 * @file log.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核诊断日志与致命错误输出
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/early_console.h>
#include <log.h>
#include <synchronized.h>

#include <cstddef>

namespace kernel::log {
    namespace {
        constinit synchronized<Logger> logger;
    }

    int __writes(const char *data, size_t sz) noexcept {
        for (size_t i = 0; i < sz; ++i) {
            putc(data[i]);
        }
        return static_cast<int>(sz);
    }

    int Output::operator()(const char *data, size_t sz) const noexcept {
        return __writes(data, sz);
    }

    locked_ref<Logger> global() noexcept {
        return logger.lock();
    }

    void putc(char ch) noexcept {
        hal::early_console().putc(ch);
    }

    [[noreturn]] void halt() noexcept {
        hal::early_console().halt();
    }
}  // namespace kernel::log

[[noreturn]] void tay::panic(const char *message) noexcept {
    kernel::log::panic("{}", message);
}
