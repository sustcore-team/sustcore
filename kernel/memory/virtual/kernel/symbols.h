/**
 * @file symbols.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核映像分页布局所需的链接脚本边界
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

namespace memory::detail {
    extern "C" char s_init[], e_init[];
    extern "C" char s_init_text[], e_init_text[];
    extern "C" char s_init_rodata[], e_init_rodata[];
    extern "C" char s_init_data[], e_init_data[];
    extern "C" char s_init_bss[], e_init_bss[];
    extern "C" char s_text[], e_text[];
    extern "C" char s_rodata[], e_rodata[];
    extern "C" char s_data[], e_data[];
    extern "C" char s_bss[], e_bss[];
    extern "C" char __bsp_stack_bottom[], __bsp_stack_top[];
    extern "C" char skernel[], ekernel[];
}  // namespace memory::detail
