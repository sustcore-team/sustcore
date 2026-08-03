/**
 * @file kinit.h
 * @brief 当前阶段的内核线程 bring-up 程序
 */

#pragma once

namespace init {
    [[noreturn]] void run_kinit() noexcept;
}
