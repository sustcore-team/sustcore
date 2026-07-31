#include <tay/logger.h>

#include <cstddef>

namespace {
    char output[256]{};
    std::size_t output_size = 0;

    struct log_output {
        int operator()(const char* data, std::size_t size) {
            for (std::size_t index = 0; index < size; ++index) {
                output[output_size++] = data[index];
            }
            return static_cast<int>(size);
        }
    };

    using loggers = tay::logger<log_output>;
}  // namespace

int main() {
    loggers logger;
    auto debug = logger.debug("boot={}", true);
    auto error = logger.error("cause={:x}", 42u);
    return debug && error && output_size != 0 ? 0 : 1;
}
