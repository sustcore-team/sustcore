/**
 * @file panic.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 声明适配目标环境的致命错误入口。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
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
