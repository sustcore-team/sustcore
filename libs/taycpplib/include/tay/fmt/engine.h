/**
 * @file engine.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现编译期格式校验和回调格式化引擎。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/expected.h>
#include <tay/fmt/context.h>
#include <tay/fmt/core.h>
#include <tay/fmt/formatters.h>

#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace tay::detail {
    enum class parse_error {
        none,
        unmatched_open,
        unescaped_close,
        index_overflow,
        argument_out_of_range,
        mixed_argument_indexing,
        invalid_format_spec,
        missing_formatter,
    };

    enum class indexing_mode {
        unknown,
        automatic,
        manual,
    };

    template <class Formatter>
    concept has_parse = requires(Formatter value, format_parse_context& context) {
        {
            value.parse(context)
        } -> std::same_as<format_parse_context::iterator>;
    };

    inline constexpr parse_error validate_argument_spec(size_t, string_view) noexcept {
        return parse_error::argument_out_of_range;
    }

    template <class Head, class... Tail>
    constexpr parse_error validate_argument_spec(size_t index, string_view spec) noexcept {
        if (index != 0) {
            if constexpr (sizeof...(Tail) == 0) {
                return parse_error::argument_out_of_range;
            } else {
                return validate_argument_spec<Tail...>(index - 1, spec);
            }
        }

        using formatter_type = formatter<format_arg_t<Head>>;
        if constexpr (!has_parse<formatter_type>) {
            return parse_error::missing_formatter;
        } else {
            formatter_type selected{};
            format_parse_context context(spec);
            const auto consumed = selected.parse(context);
            return consumed == context.end() ? parse_error::none : parse_error::invalid_format_spec;
        }
    }

    template <class LiteralHandler, class FieldHandler>
    constexpr parse_error scan_format(string_view text, LiteralHandler&& literal,
                                      FieldHandler&& field) {
        size_t position          = 0;
        size_t next_argument     = 0;
        indexing_mode index_mode = indexing_mode::unknown;

        while (position < text.size()) {
            const size_t literal_start = position;
            while (position < text.size() && text[position] != '{' && text[position] != '}') {
                ++position;
            }
            if (position != literal_start) {
                literal(string_view(text.data() + literal_start, position - literal_start));
            }
            if (position == text.size()) {
                return parse_error::none;
            }

            if (text[position] == '}') {
                if (position + 1 < text.size() && text[position + 1] == '}') {
                    literal(string_view("}", 1));
                    position += 2;
                    continue;
                }
                return parse_error::unescaped_close;
            }

            if (position + 1 < text.size() && text[position + 1] == '{') {
                literal(string_view("{", 1));
                position += 2;
                continue;
            }

            ++position;
            bool has_index  = false;
            size_t argument = 0;

            while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
                has_index          = true;
                const size_t digit = static_cast<size_t>(text[position] - '0');
                if (argument > (std::numeric_limits<size_t>::max() - digit) / 10) {
                    return parse_error::index_overflow;
                }
                argument = argument * 10 + digit;
                ++position;
            }

            if (has_index) {
                if (index_mode == indexing_mode::automatic) {
                    return parse_error::mixed_argument_indexing;
                }
                index_mode = indexing_mode::manual;
            } else {
                if (index_mode == indexing_mode::manual) {
                    return parse_error::mixed_argument_indexing;
                }
                index_mode = indexing_mode::automatic;
                argument   = next_argument++;
            }

            size_t spec_start = position;
            if (position < text.size() && text[position] == ':') {
                spec_start = ++position;
                while (position < text.size() && text[position] != '}') {
                    if (text[position] == '{') {
                        return parse_error::invalid_format_spec;
                    }
                    ++position;
                }
            }

            if (position >= text.size() || text[position] != '}') {
                return parse_error::unmatched_open;
            }

            const string_view spec(text.data() + spec_start, position - spec_start);
            const parse_error field_error = field(argument, spec);
            if (field_error != parse_error::none) {
                return field_error;
            }
            ++position;
        }
        return parse_error::none;
    }

    [[noreturn]] inline void fail_unmatched_open() {
        __builtin_abort();
    }

    [[noreturn]] inline void fail_unescaped_close() {
        __builtin_abort();
    }

    [[noreturn]] inline void fail_index_overflow() {
        __builtin_abort();
    }

    [[noreturn]] inline void fail_argument_out_of_range() {
        __builtin_abort();
    }

    [[noreturn]] inline void fail_mixed_argument_indexing() {
        __builtin_abort();
    }

    [[noreturn]] inline void fail_invalid_format_spec() {
        __builtin_abort();
    }

    [[noreturn]] inline void fail_missing_formatter() {
        __builtin_abort();
    }

    consteval void report_parse_error(parse_error error) {
        switch (error) {
            case parse_error::unmatched_open:          fail_unmatched_open();
            case parse_error::unescaped_close:         fail_unescaped_close();
            case parse_error::index_overflow:          fail_index_overflow();
            case parse_error::argument_out_of_range:   fail_argument_out_of_range();
            case parse_error::mixed_argument_indexing: fail_mixed_argument_indexing();
            case parse_error::invalid_format_spec:     fail_invalid_format_spec();
            case parse_error::missing_formatter:       fail_missing_formatter();
            case parse_error::none:                    break;
        }
    }

    template <class... Args>
    consteval void validate_format(string_view text) {
        const auto ignore_literal = [](string_view) constexpr {};
        const auto validate_field = [](size_t index, string_view spec) constexpr {
            if constexpr (sizeof...(Args) == 0) {
                return parse_error::argument_out_of_range;
            } else {
                return validate_argument_spec<Args...>(index, spec);
            }
        };
        const parse_error error = scan_format(text, ignore_literal, validate_field);
        if (error != parse_error::none) {
            report_parse_error(error);
        }
    }
}  // namespace tay::detail

namespace tay {
    template <class... Args>
    template <size_t N>
    consteval format_string<Args...>::format_string(const char (&text)[N]) noexcept
        : text_(text, N - 1) {
        detail::validate_format<Args...>(text_);
    }
}  // namespace tay

namespace tay::detail {

    template <size_t ChunkSize, class Callback>
    class callback_sink {
    private:
        Callback* callback_ = nullptr;
        char chunk_[ChunkSize];
        size_t offset_      = 0;
        size_t generated_   = 0;
        bool sink_failed_   = false;
        bool size_overflow_ = false;

        void count_character() noexcept {
            if (generated_ == std::numeric_limits<size_t>::max()) {
                size_overflow_ = true;
            } else {
                ++generated_;
            }
        }

        void flush() {
            if (offset_ == 0 || stopped()) {
                return;
            }
            const auto written = (*callback_)(chunk_, offset_);
            if (written < 0 || static_cast<size_t>(written) != offset_) {
                sink_failed_ = true;
            }
            offset_ = 0;
        }

    public:
        explicit callback_sink(Callback& callback) noexcept : callback_(&callback) {}

        void put(char character) {
            if (stopped()) {
                return;
            }
            count_character();
            chunk_[offset_++] = character;
            if (offset_ == ChunkSize) {
                flush();
            }
        }

        void write(const char* data, size_t size) {
            for (size_t index = 0; index < size; ++index) {
                put(data[index]);
            }
        }

        void finish() {
            flush();
        }

        [[nodiscard]] bool stopped() const noexcept {
            return sink_failed_ || size_overflow_;
        }

        [[nodiscard]] expected<size_t, format_error> result() const {
            if (size_overflow_) {
                return expected<size_t, format_error>(unexpect, format_error::output_size_overflow);
            }
            if (sink_failed_) {
                return expected<size_t, format_error>(unexpect, format_error::sink_error);
            }
            return generated_;
        }
    };

    template <class Context>
    void format_argument(size_t, string_view, Context&) {}

    template <class Context, class Head, class... Tail>
    void format_argument(size_t index, string_view spec, Context& context, Head&& head,
                         Tail&&... tail) {
        if (index != 0) {
            if constexpr (sizeof...(Tail) != 0) {
                format_argument(index - 1, spec, context, std::forward<Tail>(tail)...);
            }
            return;
        }

        using formatter_type = formatter<format_arg_t<Head>>;
        if constexpr (has_parse<formatter_type>) {
            formatter_type selected{};
            format_parse_context parse_context(spec);
            selected.parse(parse_context);
            selected.format(std::forward<Head>(head), context);
        }
    }

    template <class Context, class... Args>
    void execute_format(Context& context, string_view text, Args&&... args) {
        const auto write_literal = [&context](string_view literal) {
            if (!context.stopped()) {
                context.write(literal);
            }
        };
        const auto write_field = [&context, &args...](size_t index, string_view spec) {
            if (!context.stopped()) {
                format_argument(index, spec, context, std::forward<Args>(args)...);
            }
            return parse_error::none;
        };
        (void)scan_format(text, write_literal, write_field);
    }

    template <class Sink>
    template <class... Args>
    typename basic_format_context<Sink>::iterator basic_format_context<Sink>::format(
        format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        execute_format(*this, fmt.get(), std::forward<Args>(args)...);
        return out();
    }
}  // namespace tay::detail
