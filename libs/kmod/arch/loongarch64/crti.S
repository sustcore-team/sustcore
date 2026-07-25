# CRTI.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 提供init节与fini节的头代码(Prologue)

    .section .init, "ax", @progbits
    .balign 4
    .global _init
    .type _init, @function
_init:

    .section .fini, "ax", @progbits
    .balign 4
    .global _fini
    .type _fini, @function
_fini:
