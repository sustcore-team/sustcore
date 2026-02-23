# CRT0.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 入口点 _start

.extern _cpp_setup
    .text
    .global _start
    .type   _start, @function
_start:
    .option push
    .option norelax
    1:  auipc gp, %pcrel_hi(__global_pointer$)
        addi  gp, gp, %pcrel_lo(1b)
    .option pop
    /* 直接跳转到 _cpp_setup */
    j _cpp_setup
