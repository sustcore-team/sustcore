/**
 * @file logger.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供带源码位置的类型化日志接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/expected.h>
#include <tay/format.h>
#include <tay/string_view.h>
#include <tay/utility.h>

#include <concepts>
#include <cstddef>
#include <limits>
#include <source_location>
#include <type_traits>
#include <utility>

namespace tay {
    namespace detail {
        struct logger_output_tag {};
    }  // namespace detail

    namespace detail {
        inline constexpr string_view logger_header_path(__FILE__);
        inline constexpr string_view logger_root_marker("libs/taycpplib/include/tay/logger.h");
        inline constexpr size_t logger_project_root_length =
            logger_header_path.find(logger_root_marker);

        [[nodiscard]] constexpr size_t logger_rfind(string_view text, string_view pattern,
                                                    size_t end) noexcept {
            if (end > text.size()) {
                end = text.size();
            }
            if (pattern.empty()) {
                return end;
            }
            if (pattern.size() > end) {
                return string_view::npos;
            }

            const size_t last = end - pattern.size();
            for (size_t candidate = last + 1; candidate > 0; --candidate) {
                const size_t position = candidate - 1;
                bool matched          = true;
                for (size_t index = 0; index < pattern.size(); ++index) {
                    if (text[position + index] != pattern[index]) {
                        matched = false;
                        break;
                    }
                }
                if (matched) {
                    return position;
                }
            }
            return string_view::npos;
        }

        [[nodiscard]] constexpr string_view logger_relative_file(const char* file_name) noexcept {
            const string_view path(file_name);
            if constexpr (logger_project_root_length == string_view::npos ||
                          logger_project_root_length == 0)
            {
                return path;
            } else {
                if (path.size() < logger_project_root_length) {
                    return path;
                }
                for (size_t index = 0; index < logger_project_root_length; ++index) {
                    if (path[index] != logger_header_path[index]) {
                        return path;
                    }
                }
                return string_view(path.data() + logger_project_root_length,
                                   path.size() - logger_project_root_length);
            }
        }

        [[nodiscard]] constexpr string_view logger_function_name(
            const char* function_name) noexcept {
            const string_view function(function_name);
            const size_t parenthesis = function.rfind('(');
            size_t end = parenthesis == string_view::npos ? function.size() : parenthesis;
            while (end > 0 && function[end - 1] == ' ') {
                --end;
            }

            const size_t scope = logger_rfind(function, "::", end);
            if (scope != string_view::npos) {
                return string_view(function.data() + scope + 2, end - scope - 2);
            }

            const size_t operator_name = logger_rfind(function, "operator ", end);
            if (operator_name != string_view::npos) {
                return string_view(function.data() + operator_name, end - operator_name);
            }

            size_t begin = end;
            while (begin > 0 && function[begin - 1] != ' ') {
                --begin;
            }
            return string_view(function.data() + begin, end - begin);
        }
    }  // namespace detail

    enum class log_level {
        DEBUG = 0,
        INFO,
        WARN,
        ERROR,
        FATAL,
        DISABLE,
    };

    template <class... Args>
    class logger_format_string {
    private:
        format_string<Args...> format_;
        std::source_location location_;

    public:
        template <size_t N>
        consteval logger_format_string(
            const char (&text)[N],
            std::source_location location = std::source_location::current()) noexcept
            : format_(text), location_(location) {}

        [[nodiscard]] constexpr format_string<Args...> format() const noexcept {
            return format_;
        }

        [[nodiscard]] constexpr std::source_location location() const noexcept {
            return location_;
        }
    };

    template <class Output, log_level MinimumLevel = log_level::DEBUG, size_t ChunkSize = 256>
    class logger : private composition<detail::logger_output_tag, Output> {
    public:
        using output_type = Output;
        using result_type = expected<size_t, format_error>;

        static constexpr log_level minimum_level = MinimumLevel;
        static constexpr size_t chunk_size       = ChunkSize;

    private:
        static_assert(ChunkSize > 0, "tay::logger requires a non-zero chunk size");
        template <log_level Level>
        [[nodiscard]] static constexpr string_view level_text() noexcept {
            if constexpr (Level == log_level::DEBUG) {
                return "\x1b[34mDEBUG\x1b[0m";
            } else if constexpr (Level == log_level::INFO) {
                return "\x1b[32mINFO\x1b[0m";
            } else if constexpr (Level == log_level::WARN) {
                return "\x1b[35mWARN\x1b[0m";
            } else if constexpr (Level == log_level::ERROR) {
                return "\x1b[31mERROR\x1b[0m";
            } else if constexpr (Level == log_level::FATAL) {
                return "\x1b[31mFATAL\x1b[0m";
            } else {
                return "UNKNOWN";
            }
        }

        [[nodiscard]] static result_type add_size(size_t& total, result_type part) {
            if (!part) {
                return result_type(unexpect, part.error());
            }
            if (*part > std::numeric_limits<size_t>::max() - total) {
                return result_type(unexpect, format_error::output_size_overflow);
            }
            total += *part;
            return total;
        }

        [[nodiscard]] constexpr output_type& output() noexcept {
            return get<detail::logger_output_tag>(this);
        }

        template <log_level Level, class... Args>
        [[nodiscard]] result_type write(logger_format_string<std::type_identity_t<Args>...> fmt,
                                        Args&&... args) {
            if constexpr (MinimumLevel == log_level::DISABLE || Level < MinimumLevel) {
                return size_t{0};
            } else {
                size_t total = 0;

                auto prefix = tay::format_to<ChunkSize>(
                    output(),
                    "({}:{}:{})[{}]: ", detail::logger_relative_file(fmt.location().file_name()),
                    fmt.location().line(),
                    detail::logger_function_name(fmt.location().function_name()),
                    level_text<Level>());
                auto accumulated = add_size(total, std::move(prefix));
                if (!accumulated) {
                    return accumulated;
                }

                auto message =
                    tay::format_to<ChunkSize>(output(), fmt.format(), std::forward<Args>(args)...);
                accumulated = add_size(total, std::move(message));
                if (!accumulated) {
                    return accumulated;
                }

                auto newline = tay::format_to<ChunkSize>(output(), "\n");
                accumulated  = add_size(total, std::move(newline));
                if (!accumulated) {
                    return accumulated;
                }
                return total;
            }
        }

    public:
        constexpr logger() noexcept(std::is_nothrow_default_constructible_v<output_type>)
            requires std::is_default_constructible_v<output_type>
        = default;

        template <typename U>
            requires std::constructible_from<output_type, U&&>
        constexpr explicit logger(U&& output) noexcept(
            std::is_nothrow_constructible_v<output_type, U&&>)
            : composition<detail::logger_output_tag, output_type>(std::forward<U>(output)) {}

        template <class... Args>
        result_type debug(logger_format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
            return write<log_level::DEBUG>(fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        result_type info(logger_format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
            return write<log_level::INFO>(fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        result_type warn(logger_format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
            return write<log_level::WARN>(fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        result_type error(logger_format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
            return write<log_level::ERROR>(fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        result_type fatal(logger_format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
            return write<log_level::FATAL>(fmt, std::forward<Args>(args)...);
        }
    };
}  // namespace tay
