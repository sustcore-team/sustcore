/**
 * @file crtn.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief riscv64 下 kmodlib 的 crtn.o 文件汇编实现
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

# CRTN.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 提供init节与fini节的尾代码(Epilogue)
# crtn.S - 提供 _init 和 _fini 函数的结尾部分

.section .init
	ret

.section .fini
	ret
