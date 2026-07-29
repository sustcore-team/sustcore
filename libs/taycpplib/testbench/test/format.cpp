#include <tay/allocator.h>
#include <tay/format.h>
#include <tay/string.h>
#include <tay/string_view.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace {
    struct collecting_sink {
        enum class failure_mode {
            none,
            negative,
            short_write,
            over_write,
        };

        char buffer[4096]{};
        std::size_t size      = 0;
        std::size_t calls     = 0;
        std::size_t fail_call = std::size_t(-1);
        failure_mode failure  = failure_mode::none;

        int operator()(const char* data, std::size_t length) {
            const std::size_t current_call = calls++;
            assert(size + length < sizeof(buffer));
            std::memcpy(buffer + size, data, length);
            size         += length;
            buffer[size]  = '\0';

            if (current_call != fail_call) {
                return static_cast<int>(length);
            }
            switch (failure) {
                case failure_mode::negative: return -1;
                case failure_mode::short_write:
                    return static_cast<int>(length - 1);
                case failure_mode::over_write:
                    return static_cast<int>(length + 1);
                case failure_mode::none: break;
            }
            return static_cast<int>(length);
        }

        void clear() {
            buffer[0] = '\0';
            size      = 0;
            calls     = 0;
            fail_call = std::size_t(-1);
            failure   = failure_mode::none;
        }
    };

    struct move_only_value {
        char character;

        explicit move_only_value(char value) : character(value) {}
        move_only_value(const move_only_value&)            = delete;
        move_only_value& operator=(const move_only_value&) = delete;
    };

    struct cache_t {
        unsigned size;
        bool enabled;

        cache_t(unsigned cache_size, bool cache_enabled)
            : size(cache_size), enabled(cache_enabled) {}
        cache_t(const cache_t&)            = delete;
        cache_t& operator=(const cache_t&) = delete;
    };

    struct cpu_status {
        unsigned id;
        bool ready;
        unsigned cause;
        cache_t cache;
    };
}  // namespace

namespace tay {
    template <>
    struct formatter<move_only_value> {
        bool doubled = false;

        constexpr format_parse_context::iterator parse(
            format_parse_context& context) noexcept {
            auto position = context.begin();
            if (position == context.end()) {
                return position;
            }
            if (*position != 'd') {
                return position;
            }
            doubled = true;
            ++position;
            context.advance_to(position);
            return position;
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const move_only_value& value,
                                                FormatContext& context) const {
            auto output = context.out();
            *output++   = value.character;
            if (doubled) {
                *output++ = value.character;
            }
            return output;
        }
    };

    template <>
    struct formatter<cache_t> {
        constexpr format_parse_context::iterator parse(
            format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const cache_t& cache,
                                                FormatContext& context) const {
            context.write("cache{size=");
            context.format(cache.size);
            context.write(", enabled=");
            context.template format<bool>(cache.enabled);
            context.put('}');
            return context.out();
        }
    };

    template <>
    struct formatter<cpu_status> {
        constexpr format_parse_context::iterator parse(
            format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const cpu_status& status,
                                                FormatContext& context) const {
            context.write("cpu_status{id=");
            context.format(status.id);
            context.write(", ready=");
            context.format(status.ready);
            context.write(", cause=");
            context.template format<unsigned>(status.cause);
            context.write(", cache=");
            context.format(status.cache);
            context.put('}');
            return context.out();
        }
    };
}  // namespace tay

namespace {
    template <class Context, class Value>
    concept supports_context_format_spec =
        requires(Context& context, Value&& value) {
            context.format(std::forward<Value>(value), tay::string_view("x"));
        };

    static_assert(!supports_context_format_spec<
                  tay::detail::basic_format_context<collecting_sink>,
                  unsigned>);

    void expect_text(const collecting_sink& sink, const char* expected) {
        assert(std::strcmp(sink.buffer, expected) == 0);
        assert(sink.size == std::strlen(expected));
    }

    void parser_and_builtin_formatters_work() {
        collecting_sink sink;

        auto result = tay::format_to(sink, "plain text");
        assert(result && *result == 10);
        expect_text(sink, "plain text");

        sink.clear();
        result = tay::format_to(sink, "{{{0}}} {1} {0}", "left", "right");
        assert(result && *result == 17);
        expect_text(sink, "{left} right left");

        sink.clear();
        result = tay::format_to(sink, "{} {:d} {:b} {:o} {:x} {:X}", -42, -42,
                                42u, 42u, 42u, 42u);
        assert(result);
        expect_text(sink, "-42 -42 101010 52 2a 2A");

        sink.clear();
        result = tay::format_to(sink, "{:b} {:o} {:x} {:X}", -5, -9, -42, -42);
        assert(result);
        expect_text(sink, "-101 -11 -2a -2A");

        sink.clear();
        result = tay::format_to(sink, "{} {} {}",
                                std::numeric_limits<std::int8_t>::min(),
                                std::numeric_limits<std::uint64_t>::max(),
                                static_cast<signed char>(-7));
        assert(result);
        expect_text(sink, "-128 18446744073709551615 -7");

        sink.clear();
        result = tay::format_to(sink, "{:b}",
                                std::numeric_limits<std::uint64_t>::max());
        assert(result && *result == 64);
        expect_text(
            sink,
            "1111111111111111111111111111111111111111111111111111111111111111");

        sink.clear();
        result =
            tay::format_to(sink, "{} {:b} {:d} {}", true, false, true, 'Z');
        assert(result);
        expect_text(sink, "true false 1 Z");

        sink.clear();
        const char* null_text = nullptr;
        const tay::string_view view("view");
        tay::string<tay::allocator<char>> owned("owned");
        result = tay::format_to(sink, "{} {} {} {}", "literal", null_text, view,
                                owned);
        assert(result);
        expect_text(sink, "literal (null) view owned");

        sink.clear();
        void* pointer = reinterpret_cast<void*>(std::uintptr_t(0x1234));
        result        = tay::format_to(sink, "{} {}", pointer, nullptr);
        assert(result);
        expect_text(sink, "0x1234 0x0");
    }

    void chunking_and_failures_work() {
        collecting_sink sink;

        auto result = tay::format_to<1>(sink, "abcd");
        assert(result && *result == 4 && sink.calls == 4);
        expect_text(sink, "abcd");

        sink.clear();
        result = tay::format_to<4>(sink, "abcdefghij");
        assert(result && *result == 10 && sink.calls == 3);
        expect_text(sink, "abcdefghij");

        sink.clear();
        result = tay::format_to<4>(sink, "abcdefgh");
        assert(result && *result == 8 && sink.calls == 2);
        expect_text(sink, "abcdefgh");

        sink.clear();
        result = tay::format_to<4>(sink, "");
        assert(result && *result == 0 && sink.calls == 0);

        char long_text[301]{};
        for (std::size_t index = 0; index < sizeof(long_text) - 1; ++index) {
            long_text[index] = static_cast<char>('a' + index % 26);
        }
        sink.clear();
        result = tay::format_to<64>(sink, "{}", long_text);
        assert(result && *result == 300 && sink.calls == 5);
        assert(std::memcmp(sink.buffer, long_text, 300) == 0);

        for (std::size_t fail_call = 0; fail_call < 3; ++fail_call) {
            sink.clear();
            sink.fail_call = fail_call;
            sink.failure   = collecting_sink::failure_mode::negative;
            result         = tay::format_to<4>(sink, "abcdefghijkl");
            assert(!result);
            assert(result.error() == tay::format_error::sink_error);
            assert(sink.calls == 3);
            expect_text(sink, "abcdefghijkl");
        }

        for (const auto mode : {collecting_sink::failure_mode::short_write,
                                collecting_sink::failure_mode::over_write})
        {
            sink.clear();
            sink.fail_call = 1;
            sink.failure   = mode;
            result         = tay::format_to<4>(sink, "abcdefghijkl");
            assert(!result);
            assert(result.error() == tay::format_error::sink_error);
            assert(sink.calls == 3);
            expect_text(sink, "abcdefghijkl");
        }
    }

    void custom_formatter_does_not_copy_arguments() {
        collecting_sink sink;
        move_only_value value('q');
        auto result = tay::format_to(sink, "{} {:d}", value, value);
        assert(result && *result == 4);
        expect_text(sink, "q qq");
    }

    void context_format_supports_explicit_and_nested_formatters() {
        collecting_sink sink;
        cpu_status status{3, true, 42, cache_t(64, false)};

        auto result = tay::format_to(sink, "{}", status);
        assert(result);
        expect_text(sink,
                    "cpu_status{id=3, ready=true, cause=42, "
                    "cache=cache{size=64, enabled=false}}");
    }
}  // namespace

int main() {
    parser_and_builtin_formatters_work();
    chunking_and_failures_work();
    custom_formatter_does_not_copy_arguments();
    context_format_supports_explicit_and_nested_formatters();
    return 0;
}
