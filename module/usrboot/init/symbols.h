/**
 * @file symbols.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief usrboot symbols
 * @version 0.1.0-dev.1
 * @date 2026-08-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

extern "C" char s_bss[], e_bss[];
extern "C" void __early_clear(void *begin, void *end) noexcept;

using initializer_t = void (*)();
extern "C" initializer_t __preinit_array_start[], __preinit_array_end[];
extern "C" initializer_t __init_array_start[], __init_array_end[];
