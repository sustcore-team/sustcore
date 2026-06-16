
    .section .text, "ax", @progbits
    .globl __switch_to
    .type __switch_to, @function
    .globl new_utask_trampoline
    .type new_utask_trampoline, @function
__switch_to:
new_utask_trampoline:
    b .

