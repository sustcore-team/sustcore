/**
 * @file panic.h
 * @brief Environment-aware fatal error entry point.
 */

#pragma once

#if defined(TAY_ENV_HOST)
#include <cstdio>
#include <cstdlib>
#endif

namespace tay {
#if defined(TAY_ENV_HOST)
    [[noreturn]] inline void panic(const char *message) noexcept {
        if (message != nullptr) {
            std::fputs(message, stderr);
        }
        std::fputc('\n', stderr);
        std::fflush(stderr);
        std::abort();
    }
#elif defined(TAY_ENV_FREESTANDING)
    [[noreturn]] void panic(const char *message) noexcept;
#else
#error "tay/panic.h requires TAY_ENV_HOST or TAY_ENV_FREESTANDING"
#endif
}  // namespace tay
