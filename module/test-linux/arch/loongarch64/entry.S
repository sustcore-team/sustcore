    .text
    .global _start
    .type _start, @function
    .extern test_linux_main

_start:
    move $t0, $sp
    ld.d $a0, $t0, 0
    addi.d $a1, $t0, 8
    slli.d $t2, $a0, 3
    addi.d $t1, $a1, 8
    add.d $a2, $t1, $t2
    b test_linux_main
