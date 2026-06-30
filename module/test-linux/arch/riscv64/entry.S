    .text
    .global _start
    .type _start, @function
    .extern test_linux_main

_start:
    .option push
    .option norelax
1:
    auipc gp, %pcrel_hi(__global_pointer$)
    addi gp, gp, %pcrel_lo(1b)
    .option pop
    mv t0, sp
    ld a0, 0(t0)
    addi a1, t0, 8
    addi t1, a1, 8
    slli t2, a0, 3
    add t1, t1, t2
    mv a2, t1
    j test_linux_main
