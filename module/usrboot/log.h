/**
 * @file log.h
 * @brief usrboot 的格式化日志接口。
 */

#pragma once

#include <tay/logger.h>
#include <usrboot_syscall.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace logger {
    struct Output {
        int operator()(const char *data, size_t size) const noexcept;
    };

    using Logger = tay::logger<Output, tay::log_level::DEBUG, 256>;

    [[nodiscard]] Logger &global() noexcept;

    template <class... Args>
    void debug(tay::logger_format_string<std::type_identity_t<Args>...> format,
               Args &&...args) noexcept {
        (void)global().debug(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void info(tay::logger_format_string<std::type_identity_t<Args>...> format,
              Args &&...args) noexcept {
        (void)global().info(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void warn(tay::logger_format_string<std::type_identity_t<Args>...> format,
              Args &&...args) noexcept {
        (void)global().warn(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void error(tay::logger_format_string<std::type_identity_t<Args>...> format,
               Args &&...args) noexcept {
        (void)global().error(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    [[noreturn]] void panic(tay::logger_format_string<std::type_identity_t<Args>...> format,
                            Args &&...args) noexcept {
        (void)global().fatal(format, std::forward<Args>(args)...);
        while (true) usrboot_yield();
    }
}  // namespace logger
