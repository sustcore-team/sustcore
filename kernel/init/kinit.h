/**
 * @file kinit.h
 * @brief 当前阶段的内核线程 bring-up 程序
 */

#pragma once

#include <init/usrboot_error.h>
#include <tay/err.h>
#include <tay/expected.h>

namespace init {
    [[nodiscard]] tay::expected<void, UsrbootError> start_usrboot() noexcept;
    [[noreturn]] void run_kinit() noexcept;
}  // namespace init
