/**
 * @file format.cpp
 * @brief Demonstrate allocation-free callback formatting.
 */

#include <tay/format.h>

#include <cstddef>
#include <cstdio>

namespace {
    struct cache_t {
        unsigned size;
        bool enabled;
    };

    struct cpu_status {
        unsigned id;
        bool ready;
        unsigned cause;
        cache_t cache;
    };

    struct stdout_writer {
        std::size_t calls = 0;

        int operator()(const char* data, std::size_t size) {
            ++calls;
            return static_cast<int>(std::fwrite(data, 1, size, stdout));
        }
    };
}  // namespace

namespace tay {
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
            context.format(cache.enabled);
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
            context.template format<unsigned>(status.id);
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

int main() {
    stdout_writer writer;
    cpu_status status{3, true, 0x2a, cache_t{64, true}};

    auto result = tay::format_to<16>(
        writer, "cpu={} ready={} cause={:x}\ncustom={} address={}\n", status.id,
        status.ready, status.cause, status, &status);

    if (!result) {
        std::fprintf(stderr, "format_to failed with error code %u\n",
                     static_cast<unsigned>(result.error()));
        return 1;
    }

    std::printf("format_to emitted %zu bytes in %zu callback calls\n", *result,
                writer.calls);
    return 0;
}
