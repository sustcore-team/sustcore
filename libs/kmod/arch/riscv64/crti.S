/**
 * @file crti.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief riscv64 下 kmodlib 的 crti.o 文件汇编实现
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

# CRTI.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 提供init节与fini节的头代码(Prologue)

/* _init */
    .section .init, "ax", @progbits
    .balign 4
    .global _init
    .type   _init, @function
_init:
    /* 功能由 init_array 接管 */
    /* 即使没有 init_array 接管, GCC也会帮我们把初始化代码放置于此处 */

/* _fini */
    .section .fini, "ax", @progbits
    .balign 4
    .global _fini
    .type   _fini, @function
_fini:
    /* 同样, 这里是空的 */
