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
    /* 将当前用户栈顶作为启动参数块指针传给 _cpp_setup */
    mv a0, sp
    j _cpp_setup
