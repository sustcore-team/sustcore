/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构 Trait 定义
 * @version alpha-1.0.0
 * @date 2026-01-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <concept>
#include <arch/riscv64/trait.h>

template <typename T>
concept ArchSerialTrait = requires(char ch, const char *str) {
    {
        T::serial_write_char(ch)
    } -> std::same_as<void>;
    {
        T::serial_write_string(str)
    } -> std::same_as<void>;
};

static_assert(ArchSerialTrait<ArchSerial>);

void kernel_setup(void);