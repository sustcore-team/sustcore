/**
 * @file panic.h
 * @brief Environment-aware fatal error entry point.
 */

#pragma once

#if defined(TAY_ENV_HOST) && defined(TAY_ENV_FREESTANDING)
#error "tay/panic.h received conflicting build environments"
#elif !defined(TAY_ENV_HOST) && !defined(TAY_ENV_FREESTANDING)
#error "tay/panic.h requires TAY_ENV_HOST or TAY_ENV_FREESTANDING"
#endif

namespace tay {
    [[noreturn]] void panic(const char *message) noexcept;
}  // namespace tay
