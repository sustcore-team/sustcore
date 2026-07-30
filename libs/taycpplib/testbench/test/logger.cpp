#include <tay/logger.h>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace {
    struct output_state {
        char buffer[4096]{};
        std::size_t size          = 0;
        std::size_t calls         = 0;
        std::size_t constructions = 0;
        std::size_t fail_call     = std::size_t(-1);

        void clear() noexcept {
            buffer[0]     = '\0';
            size          = 0;
            calls         = 0;
            constructions = 0;
            fail_call     = std::size_t(-1);
        }
    };

    struct collecting_output {
        inline static output_state* state = nullptr;

        collecting_output() noexcept {
            ++state->constructions;
        }

        int operator()(const char* data, std::size_t size) {
            const std::size_t call = state->calls++;
            assert(state->size + size < sizeof(state->buffer));
            std::memcpy(state->buffer + state->size, data, size);
            state->size                += size;
            state->buffer[state->size]  = '\0';
            return call == state->fail_call ? -1 : static_cast<int>(size);
        }
    };

    struct tracked_value {
        int* calls;
    };
}  // namespace

namespace tay {
    template <>
    struct formatter<tracked_value> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(const tracked_value& value,
                                                FormatContext& context) const {
            ++*value.calls;
            context.write("tracked");
            return context.out();
        }
    };
}  // namespace tay

namespace {
    using loggers = tay::logger<collecting_output>;

    void expect_text(const output_state& state, const char* expected) {
        assert(std::strcmp(state.buffer, expected) == 0);
        assert(state.size == std::strlen(expected));
    }

    void expect_log(const output_state& state, unsigned line, const char* level,
                    const char* message, bool newline = true,
                    const char* function = "main") {
        char expected[4096]{};
        const int written = std::snprintf(
            expected, sizeof(expected),
            "(libs/taycpplib/testbench/test/logger.cpp:%u:%s)[%s]: %s%s", line,
            function, level, message, newline ? "\n" : "");
        assert(written > 0);
        expect_text(state, expected);
    }

    loggers::result_type log_from_helper(unsigned& line) {
        line = __LINE__ + 1;
        return loggers::info("from helper");
    }
}  // namespace

static_assert(std::is_same_v<typename loggers::output_type, collecting_output>);
static_assert(loggers::minimum_level == tay::log_level::DEBUG);
static_assert(loggers::chunk_size == 256);

int main() {
    output_state state;
    collecting_output::state = &state;

    const unsigned debug_line = __LINE__ + 1;
    auto debug                = loggers::debug("value={:x}", 42);
    assert(debug && *debug == state.size);
    assert(state.constructions == 1);
    expect_log(state, debug_line, "\x1b[34mDEBUG\x1b[0m", "value=2a");

    state.clear();
    const unsigned info_line = __LINE__ + 1;
    auto info                = loggers::info("ready={}", true);
    assert(info && *info == state.size);
    expect_log(state, info_line, "\x1b[32mINFO\x1b[0m", "ready=true");

    state.clear();
    unsigned helper_line = 0;
    auto helper          = log_from_helper(helper_line);
    assert(helper && *helper == state.size);
    expect_log(state, helper_line, "\x1b[32mINFO\x1b[0m", "from helper", true,
               "log_from_helper");

    state.clear();
    const unsigned warning_line = __LINE__ + 1;
    auto warning                = loggers::warn("remaining={}", 3);
    assert(warning && *warning == state.size);
    expect_log(state, warning_line, "\x1b[35mWARN\x1b[0m", "remaining=3");

    state.clear();
    const unsigned error_line = __LINE__ + 1;
    auto error                = loggers::error("code={}", -5);
    assert(error && *error == state.size);
    expect_log(state, error_line, "\x1b[31mERROR\x1b[0m", "code=-5");

    state.clear();
    const unsigned fatal_line = __LINE__ + 1;
    auto fatal                = loggers::fatal("panic at {}", "boot");
    assert(fatal && *fatal == state.size);
    expect_log(state, fatal_line, "\x1b[31mFATAL\x1b[0m", "panic at boot");

    state.clear();
    state.fail_call            = 1;
    const unsigned failed_line = __LINE__ + 1;
    auto failed                = loggers::info("message={}", 7);
    assert(!failed && failed.error() == tay::format_error::sink_error);
    assert(state.calls == 2);
    expect_log(state, failed_line, "\x1b[32mINFO\x1b[0m", "message=7", false);

    state.clear();
    int formatter_calls = 0;
    tracked_value value{&formatter_calls};
    using errors_only = tay::logger<collecting_output, tay::log_level::ERROR>;
    auto suppressed   = errors_only::debug("{}", value);
    assert(suppressed && *suppressed == 0);
    assert(formatter_calls == 0);
    assert(state.constructions == 0 && state.calls == 0 && state.size == 0);

    const unsigned emitted_line = __LINE__ + 1;
    auto emitted                = errors_only::error("{}", value);
    assert(emitted && formatter_calls == 1);
    expect_log(state, emitted_line, "\x1b[31mERROR\x1b[0m", "tracked");

    state.clear();
    using disabled = tay::logger<collecting_output, tay::log_level::DISABLE>;
    auto ignored   = disabled::fatal("not emitted");
    assert(ignored && *ignored == 0 && state.constructions == 0);
    return 0;
}
