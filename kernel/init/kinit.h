/**
 * @file kinit.h
 * @brief 当前阶段的内核线程 bring-up 程序
 */

#pragma once

#include <tay/err.h>
#include <tay/expected.h>

namespace init {
    [[nodiscard]] tay::expected<void, tay::error_code> start_usrboot() noexcept;
    [[noreturn]] void run_kinit() noexcept;
}  // namespace init
