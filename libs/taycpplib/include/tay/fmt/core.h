/**
 * @file core.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay 格式化设施的公共基础组件。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/string_view.h>

#include <cstddef>
#include <type_traits>

namespace tay {
    enum class format_error {
        sink_error,
        output_size_overflow,
        invalid_format,
        argument_out_of_range,
        mixed_argument_indexing,
        invalid_format_spec,
    };

    class format_parse_context {
    public:
        using iterator = const char *;

    private:
        string_view spec_{};
        iterator current_ = nullptr;

    public:
        constexpr explicit format_parse_context(string_view spec) noexcept
            : spec_(spec), current_(spec.begin()) {}

        [[nodiscard]] constexpr iterator begin() const noexcept {
            return current_;
        }

        [[nodiscard]] constexpr iterator end() const noexcept {
            return spec_.end();
        }

        constexpr void advance_to(iterator position) noexcept {
            current_ = position;
        }
    };

    template <class T>
    struct formatter;

    namespace detail {
        struct infer_format_type {};

        template <class T>
        struct format_arg_type_impl {
            using type = std::remove_cv_t<T>;
        };

        template <size_t N>
        struct format_arg_type_impl<char[N]> {
            using type = char *;
        };

        template <size_t N>
        struct format_arg_type_impl<const char[N]> {
            using type = const char *;
        };

        template <class T>
        using format_arg_t = typename format_arg_type_impl<std::remove_reference_t<T>>::type;
    }  // namespace detail

    template <class... Args>
    class format_string {
    private:
        string_view text_{};

    public:
        template <size_t N>
        consteval format_string(const char (&text)[N]) noexcept;

        [[nodiscard]] constexpr string_view get() const noexcept {
            return text_;
        }
    };

    namespace detail {
        template <class... Args>
        consteval void validate_format(string_view text);
    }
}  // namespace tay
