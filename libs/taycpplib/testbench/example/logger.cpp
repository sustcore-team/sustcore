/**
 * @file logger.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 类型化日志接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/logger.h>

#include <cstddef>
#include <cstdio>

struct log_output {
    int operator()(const char* data, size_t size) {
        return static_cast<int>(std::fwrite(data, 1, size, stdout));
    }
};

using loggers = tay::logger<log_output>;

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

namespace tay {
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
            context.format("{}", status.cause);
            context.write(", cache=");
            context.format("{}", status.cache);
            context.put('}');
            return context.out();
        }
    };
}  // namespace tay

int main() {
    loggers logger;
    logger.debug("boot stage={}", 1);
    logger.info("kernel image loaded at {}", reinterpret_cast<void*>(0x80200000));
    logger.warn("only {} MiB remain", 32);
    logger.error("device {} returned code {}", "virtio0", -5);
    logger.fatal("unable to continue: {}", "root filesystem missing");

    cache_t _cache{.size = 32, .enabled = true};
    cpu_status status{.id = 0, .ready = true, .cause = 0, .cache = _cache};
    logger.info("CPU status: {}", status);
    return 0;
}
