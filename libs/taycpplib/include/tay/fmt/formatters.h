/**
 * @file formatters.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供 Tay 内建类型的格式化器特化。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/bits.h>
#include <tay/fmt/core.h>
#include <tay/string_view.h>

#include <cstddef>
#include <limits>
#include <type_traits>

namespace tay::detail {
    enum class integer_presentation {
        decimal,
        binary,
        octal,
        hex,
        hex_upper,
    };

    template <class Context, class Unsigned>
    typename Context::iterator write_unsigned(Context& context, Unsigned value, unsigned radix,
                                              bool uppercase) {
        static_assert(std::is_integral_v<Unsigned> && std::is_unsigned_v<Unsigned>);

        constexpr char lower_digits[] = "0123456789abcdef";
        constexpr char upper_digits[] = "0123456789ABCDEF";
        const char* digits            = uppercase ? upper_digits : lower_digits;

        char buffer[std::numeric_limits<Unsigned>::digits + 1];
        size_t length = 0;
        do {
            const auto digit = static_cast<unsigned>(value % radix);
            buffer[length++] = digits[digit];
            value            = static_cast<Unsigned>(value / radix);
        } while (value != 0);

        while (length > 0) {
            context.put(buffer[--length]);
        }
        return context.out();
    }

    template <class Unsigned>
    [[nodiscard]] constexpr size_t unsigned_digit_count(Unsigned value, unsigned radix) noexcept {
        static_assert(std::is_integral_v<Unsigned> && std::is_unsigned_v<Unsigned>);

        size_t length = 0;
        do {
            ++length;
            value = static_cast<Unsigned>(value / radix);
        } while (value != 0);
        return length;
    }

    struct empty_spec_formatter {
        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            return context.begin();
        }
    };

    struct integer_formatter_base {
        integer_presentation presentation = integer_presentation::decimal;
        bool alternate                    = false;
        size_t zero_padding_width         = 0;

        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            auto position = context.begin();
            if (position == context.end()) {
                return position;
            }

            const auto spec_begin = position;
            alternate             = false;
            zero_padding_width    = 0;

            if (*position == '#') {
                ++position;
                alternate = true;
            }

            if (position != context.end() && *position == '0') {
                ++position;
                while (position != context.end() && *position >= '0' && *position <= '9') {
                    const size_t digit = static_cast<size_t>(*position - '0');
                    if (zero_padding_width > (std::numeric_limits<size_t>::max() - digit) / 10) {
                        return spec_begin;
                    }
                    zero_padding_width = zero_padding_width * 10 + digit;
                    ++position;
                }
            }

            if (position == context.end()) {
                return spec_begin;
            }

            switch (*position) {
                case 'd': presentation = integer_presentation::decimal; break;
                case 'b': presentation = integer_presentation::binary; break;
                case 'o': presentation = integer_presentation::octal; break;
                case 'x': presentation = integer_presentation::hex; break;
                case 'X': presentation = integer_presentation::hex_upper; break;
                default:  return position;
            }
            if (alternate && presentation != integer_presentation::hex &&
                presentation != integer_presentation::hex_upper)
            {
                return spec_begin;
            }
            ++position;
            context.advance_to(position);
            return position;
        }

        [[nodiscard]] constexpr unsigned radix() const noexcept {
            switch (presentation) {
                case integer_presentation::binary:    return 2;
                case integer_presentation::octal:     return 8;
                case integer_presentation::hex:
                case integer_presentation::hex_upper: return 16;
                default:                              return 10;
            }
        }

        [[nodiscard]] constexpr bool uppercase() const noexcept {
            return presentation == integer_presentation::hex_upper;
        }

        [[nodiscard]] constexpr bool uses_alternate_form() const noexcept {
            return alternate;
        }

        [[nodiscard]] constexpr size_t zero_padding() const noexcept {
            return zero_padding_width;
        }
    };

    template <class T>
    inline constexpr bool is_format_integer_v =
        std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
        !std::is_same_v<T, wchar_t> &&
#if defined(__cpp_char8_t)
        !std::is_same_v<T, char8_t> &&
#endif
        !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

    template <class T>
    inline constexpr bool is_character_pointer_v =
        std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>;

    template <class T>
    inline constexpr bool is_address_pointer_v =
        std::is_pointer_v<T> && !is_character_pointer_v<T> &&
        !std::is_function_v<std::remove_pointer_t<T>>;
}  // namespace tay::detail

namespace tay {
    template <class Allocator>
    class string;

    template <class T>
        requires detail::is_format_integer_v<T>
    struct formatter<T> : detail::integer_formatter_base {
        template <class FormatContext>
        typename FormatContext::iterator format(T value, FormatContext& context) const {
            using unsigned_type = std::make_unsigned_t<T>;
            unsigned_type magnitude{};
            bool negative = false;

            if constexpr (std::is_signed_v<T>) {
                if (value < 0) {
                    negative  = true;
                    magnitude = static_cast<unsigned_type>(-(value + 1));
                    ++magnitude;
                } else {
                    magnitude = static_cast<unsigned_type>(value);
                }
            } else {
                magnitude = value;
            }

            const size_t prefix_size  = this->uses_alternate_form() ? 2 : 0;
            const size_t content_size = (negative ? 1 : 0) + prefix_size +
                                        detail::unsigned_digit_count(magnitude, this->radix());
            size_t padding =
                this->zero_padding() > content_size ? this->zero_padding() - content_size : 0;

            if (negative) {
                context.put('-');
            }
            if (this->uses_alternate_form()) {
                context.write(this->uppercase() ? "0X" : "0x", 2);
            }
            while (padding > 0) {
                context.put('0');
                --padding;
            }

            return detail::write_unsigned(context, magnitude, this->radix(), this->uppercase());
        }
    };

    template <>
    struct formatter<bool> {
        bool boolalpha = true;

        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            auto position = context.begin();
            if (position == context.end()) {
                return position;
            }
            if (*position == 'b') {
                boolalpha = true;
            } else if (*position == 'd') {
                boolalpha = false;
            } else {
                return position;
            }
            ++position;
            context.advance_to(position);
            return position;
        }

        template <class FormatContext>
        typename FormatContext::iterator format(bool value, FormatContext& context) const {
            if (boolalpha) {
                context.write(value ? string_view("true") : string_view("false"));
            } else {
                context.put(value ? '1' : '0');
            }
            return context.out();
        }
    };

    template <>
    struct formatter<char> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(char value, FormatContext& context) const {
            context.put(value);
            return context.out();
        }
    };

    template <>
    struct formatter<const char*> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(const char* value, FormatContext& context) const {
            if (value == nullptr) {
                context.write("(null)", 6);
                return context.out();
            }
            while (*value != '\0') {
                context.put(*value++);
            }
            return context.out();
        }
    };

    template <>
    struct formatter<char*> : formatter<const char*> {};

    template <>
    struct formatter<string_view> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(string_view value, FormatContext& context) const {
            context.write(value);
            return context.out();
        }
    };

    template <class Allocator>
    struct formatter<string<Allocator>> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(const string<Allocator>& value,
                                                FormatContext& context) const {
            context.write(value.data(), value.size());
            return context.out();
        }
    };

    template <class T>
        requires detail::is_address_pointer_v<T>
    struct formatter<T> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(T value, FormatContext& context) const {
            context.write("0x", 2);
            return detail::write_unsigned(context, reinterpret_cast<uintptr_t>(value), 16, false);
        }
    };

    template <>
    struct formatter<decltype(nullptr)> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(decltype(nullptr), FormatContext& context) const {
            context.write("nullptr");
            return context.out();
        }
    };

    template <>
    struct formatter<error_code> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(error_code value, FormatContext& context) const {
            context.write(to_string(value));
            return context.out();
        }
    };
}  // namespace tay
