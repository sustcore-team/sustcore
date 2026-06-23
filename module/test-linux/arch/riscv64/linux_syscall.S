#include <syscall.h.in>

    .text
    .global linux_syscall
    .type linux_syscall, @function

linux_syscall:
    mv a7, a6
    ecall
    ret
