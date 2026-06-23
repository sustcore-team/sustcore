#include <syscall.h.in>

    .text
    .global linux_syscall
    .type linux_syscall, @function

linux_syscall:
    move $a7, $a6
    syscall 0
    ret
