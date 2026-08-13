/**
 * @file log.cpp
 * @brief 通过 ec_write 输出 usrboot 日志。
 */

#include <log.h>

namespace logger {
    namespace {
        constinit Logger logger;
    }

    int Output::operator()(const char *data, size_t size) const noexcept {
        return static_cast<int>(ec_write(data, size));
    }

    Logger &global() noexcept {
        return logger;
    }
}  // namespace logger
