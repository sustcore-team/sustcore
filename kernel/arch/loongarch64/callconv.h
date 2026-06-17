/**
 * @file callconv.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 系统调用调用约定
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/ctxlayout.h>
#include <syscall/packs.h>

namespace la64 {
    struct Context;

    constexpr static size_t RET0_REG      = CTX_A0_SLOT;
    constexpr static size_t RET1_REG      = CTX_A1_SLOT;
    constexpr static size_t ARG0_REG      = CTX_A0_SLOT;
    constexpr static size_t ARG1_REG      = CTX_A1_SLOT;
    constexpr static size_t ARG2_REG      = CTX_A2_SLOT;
    constexpr static size_t ARG3_REG      = CTX_A3_SLOT;
    constexpr static size_t ARG4_REG      = CTX_A4_SLOT;
    constexpr static size_t ARG5_REG      = CTX_A5_SLOT;
    constexpr static size_t ARG6_REG      = CTX_A6_SLOT;
    constexpr static size_t SYSCALLNO_REG = CTX_A7_SLOT;

    constexpr void write_ret(Context &ctx, const syscall::RetPack &pack);
    constexpr void read_args(const Context &ctx, syscall::ArgPack &pack);
    [[nodiscard]]
    constexpr syscall::ArgPack read_args(const Context &ctx);
}  // namespace la64
