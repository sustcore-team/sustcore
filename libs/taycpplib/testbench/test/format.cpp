/**
 * @file format.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 格式化接口的输出和格式串校验。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/allocator.h>
#include <tay/bits.h>
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
        size_t size          = 0;
        size_t calls         = 0;
        size_t fail_call     = size_t(-1);
        failure_mode failure = failure_mode::none;

        int operator()(const char* data, size_t length) {
            const size_t current_call = calls++;
            assert(size + length < sizeof(buffer));
            std::memcpy(buffer + size, data, length);
            size         += length;
            buffer[size]  = '\0';

            if (current_call != fail_call) {
                return static_cast<int>(length);
            }
            switch (failure) {
                case failure_mode::negative:    return -1;
                case failure_mode::short_write: return static_cast<int>(length - 1);
                case failure_mode::over_write:  return static_cast<int>(length + 1);
                case failure_mode::none:        break;
            }
            return static_cast<int>(length);
        }

        void clear() {
            buffer[0] = '\0';
            size      = 0;
            calls     = 0;
            fail_call = size_t(-1);
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

    struct tracked_value {
        int* calls;
        char character;
    };

    struct allocation_state {
        bool fail          = false;
        size_t allocations = 0;
    };

    template <class T>
    struct tracking_allocator {
        using value_type      = T;
        using is_always_equal = std::false_type;

        allocation_state* state = nullptr;

        constexpr explicit tracking_allocator(allocation_state& value) noexcept : state(&value) {}

        template <class U>
        constexpr tracking_allocator(const tracking_allocator<U>& other) noexcept
            : state(other.state) {}

        tay::expected<T*, tay::error_code> try_allocate(size_t count) noexcept {
            if (state->fail) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            auto* memory = static_cast<T*>(::operator new(count * sizeof(T), std::nothrow));
            if (memory == nullptr) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            ++state->allocations;
            return memory;
        }

        void deallocate(T* memory, size_t) noexcept {
            ::operator delete(memory);
        }

        template <class U>
        struct rebind {
            using other = tracking_allocator<U>;
        };

        template <class>
        friend struct tracking_allocator;
    };

    template <class T, class U>
    constexpr bool operator==(const tracking_allocator<T>& left,
                              const tracking_allocator<U>& right) noexcept {
        return left.state == right.state;
    }

    template <class T, class U>
    constexpr bool operator!=(const tracking_allocator<T>& left,
                              const tracking_allocator<U>& right) noexcept {
        return !(left == right);
    }

    template <class T>
    struct tiny_allocator {
        using value_type = T;

        tay::expected<T*, tay::error_code> try_allocate(size_t count) noexcept {
            auto* memory = static_cast<T*>(::operator new(count * sizeof(T), std::nothrow));
            if (memory == nullptr) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            return memory;
        }

        void deallocate(T* memory, size_t) noexcept {
            ::operator delete(memory);
        }

        constexpr size_t max_size() const noexcept {
            return 8;
        }
    };
}  // namespace

namespace tay {
    template <>
    struct formatter<move_only_value> {
        bool doubled = false;

        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
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
        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const cache_t& cache,
                                                FormatContext& context) const {
            context.write("cache{size=");
            context.format("{}", cache.size);
            context.write(", enabled=");
            context.format("{}", cache.enabled);
            context.put('}');
            return context.out();
        }
    };

    template <>
    struct formatter<cpu_status> {
        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const cpu_status& status,
                                                FormatContext& context) const {
            context.write("cpu_status{id=");
            context.format("{}", status.id);
            context.write(", ready=");
            context.format("{}", status.ready);
            context.write(", cause=");
            context.format("{:#08x}", status.cause);
            context.write(", cache=");
            context.format("{}", status.cache);
            context.put('}');
            return context.out();
        }
    };

    template <>
    struct formatter<tracked_value> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(const tracked_value& value,
                                                FormatContext& context) const {
            ++*value.calls;
            for (int index = 0; index < 6; ++index) {
                context.put(value.character);
            }
            return context.out();
        }
    };
}  // namespace tay

namespace {
    template <class Context, class Value>
    concept supports_context_format = requires(Context& context, Value&& value) {
        context.format("{:#x}", std::forward<Value>(value));
    };

    static_assert(
        supports_context_format<tay::detail::basic_format_context<collecting_sink>, unsigned>);

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
        result = tay::format_to(sink, "{} {:d} {:b} {:o} {:x} {:X}", -42, -42, 42u, 42u, 42u, 42u);
        assert(result);
        expect_text(sink, "-42 -42 101010 52 2a 2A");

        sink.clear();
        result = tay::format_to(sink, "{:b} {:o} {:x} {:X}", -5, -9, -42, -42);
        assert(result);
        expect_text(sink, "-101 -11 -2a -2A");

        sink.clear();
        result = tay::format_to(sink, "{:#x} {:#X} {:#x}", 42u, 42u, -42);
        assert(result);
        expect_text(sink, "0x2a 0X2A -0x2a");

        sink.clear();
        result = tay::format_to(sink, "{:#x} {:#X}", 0u, 0u);
        assert(result);
        expect_text(sink, "0x0 0X0");

        sink.clear();
        result = tay::format_to(sink, "{:08x} {:05d} {:05d} {:#08x}", 42u, 42, -42, 42u);
        assert(result);
        expect_text(sink, "0000002a 00042 -0042 0x00002a");

        sink.clear();
        result = tay::format_to(sink, "{} {} {}", std::numeric_limits<i8_t>::min(),
                                std::numeric_limits<u64_t>::max(), static_cast<signed char>(-7));
        assert(result);
        expect_text(sink, "-128 18446744073709551615 -7");

        sink.clear();
        result = tay::format_to(sink, "{:b}", std::numeric_limits<u64_t>::max());
        assert(result && *result == 64);
        expect_text(sink, "1111111111111111111111111111111111111111111111111111111111111111");

        sink.clear();
        result = tay::format_to(sink, "{} {:b} {:d} {}", true, false, true, 'Z');
        assert(result);
        expect_text(sink, "true false 1 Z");

        sink.clear();
        const char* null_text = nullptr;
        const tay::string_view view("view");
        tay::string<tay::allocator<char>> owned("owned");
        result = tay::format_to(sink, "{} {} {} {}", "literal", null_text, view, owned);
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
        for (size_t index = 0; index < sizeof(long_text) - 1; ++index) {
            long_text[index] = static_cast<char>('a' + index % 26);
        }
        sink.clear();
        result = tay::format_to<64>(sink, "{}", long_text);
        assert(result && *result == 300 && sink.calls == 5);
        assert(std::memcmp(sink.buffer, long_text, 300) == 0);

        for (size_t fail_call = 0; fail_call < 3; ++fail_call) {
            sink.clear();
            sink.fail_call = fail_call;
            sink.failure   = collecting_sink::failure_mode::negative;
            result         = tay::format_to<4>(sink, "abcdefghijkl");
            assert(!result);
            assert(result.error() == tay::format_error::sink_error);
            assert(sink.calls == fail_call + 1);
            const size_t expected_size = (fail_call + 1) * 4;
            assert(sink.size == expected_size);
            assert(std::memcmp(sink.buffer, "abcdefghijkl", expected_size) == 0);
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
            assert(sink.calls == 2);
            expect_text(sink, "abcdefgh");
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
                    "cpu_status{id=3, ready=true, cause=0x00002a, "
                    "cache=cache{size=64, enabled=false}}");
    }

    void bounded_iterator_output_stops_formatting() {
        char output[64];
        std::memset(output, '?', sizeof(output));

        int first_calls  = 0;
        int second_calls = 0;
        tracked_value first{&first_calls, 'a'};
        tracked_value second{&second_calls, 'b'};

        auto zero = tay::format_to_iter_s(output, 0, "{}{}", first, second);
        assert(zero && *zero == output);
        assert(first_calls == 0 && second_calls == 0 && output[0] == '?');

        auto literal = tay::format_to_iter_s(output, 3, "abcdef{}", first);
        assert(literal && *literal == output + 3);
        assert(std::memcmp(output, "abc", 3) == 0 && output[3] == '?');
        assert(first_calls == 0);

        auto formatter = tay::format_to_iter_s(output, 4, "{}{}", first, second);
        assert(formatter && *formatter == output + 4);
        assert(std::memcmp(output, "aaaa", 4) == 0 && output[4] == '?');
        assert(first_calls == 1 && second_calls == 0);

        std::memset(output, '?', sizeof(output));
        auto exact = tay::format_to_iter_s(output, 3, "{}", "xyz");
        assert(exact && *exact == output + 3);
        assert(std::memcmp(output, "xyz", 3) == 0 && output[3] == '?');

        cpu_status status{3, true, 42, cache_t(64, false)};
        auto nested = tay::format_to_iter_s(output, 18, "{}", status);
        assert(nested && *nested == output + 18);
        assert(std::memcmp(output, "cpu_status{id=3, r", 18) == 0);
    }

    void owning_format_uses_requested_allocator() {
        static_assert(std::is_same_v<decltype(tay::format("{}", 1)),
                                     tay::expected<tay::string<>, tay::error_code>>);

        auto default_result = tay::format("{} {:x} {}", "value", 42, true);
        assert(default_result && *default_result == "value 2a true");

        move_only_value value('q');
        auto custom = tay::format("{:d}", value);
        assert(custom && *custom == "qq");

        allocation_state state;
        tracking_allocator<char> allocator(state);
        auto small = tay::format(allocator, "small={}", 7);
        assert(small && *small == "small=7");
        assert(state.allocations == 0);

        auto large = tay::format(allocator, "this output is long enough to allocate: {}", 42);
        assert(large && *large == "this output is long enough to allocate: 42");
        assert(state.allocations > 0);

        state.fail  = true;
        auto failed = tay::format(allocator, "this output also requires dynamic allocation");
        assert(!failed && failed.error() == tay::error_code::OUT_OF_MEMORY);

        tiny_allocator<char> tiny;
        auto overflow = tay::format(tiny, "12345678");
        assert(!overflow);
        assert(overflow.error() == tay::error_code::ALLOCATION_SIZE_OVERFLOW);
    }
}  // namespace

int main() {
    parser_and_builtin_formatters_work();
    chunking_and_failures_work();
    custom_formatter_does_not_copy_arguments();
    context_format_supports_explicit_and_nested_formatters();
    bounded_iterator_output_stops_formatting();
    owning_format_uses_requested_allocator();
    return 0;
}
