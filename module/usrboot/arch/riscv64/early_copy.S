/**
 * @file early_copy.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 启动期内存复制原语
 * @version 0.1.0-dev.1
 * @date 2026-08-04
 *
 * @copyright Copyright (c) 2026
 *
 */

.section .text, "ax", @progbits
.p2align 4
.global __early_copy
.type __early_copy, @function
__early_copy:
    beqz    a2, .Lcopy_done

    # 只有源和目标具有相同对齐关系时，双字访问才会同时对齐。
    xor     t0, a0, a1
    andi    t0, t0, 7
    bnez    t0, .Lcopy_tail

.Lcopy_align:
    andi    t0, a0, 7
    beqz    t0, .Lcopy_cache_line
    lbu     t1, 0(a1)
    sb      t1, 0(a0)
    addi    a0, a0, 1
    addi    a1, a1, 1
    addi    a2, a2, -1
    bnez    a2, .Lcopy_align
    ret

.Lcopy_cache_line:
    li      t0, 64
    bltu    a2, t0, .Lcopy_word
    ld      t1, 0(a1)
    sd      t1, 0(a0)
    ld      t1, 8(a1)
    sd      t1, 8(a0)
    ld      t1, 16(a1)
    sd      t1, 16(a0)
    ld      t1, 24(a1)
    sd      t1, 24(a0)
    ld      t1, 32(a1)
    sd      t1, 32(a0)
    ld      t1, 40(a1)
    sd      t1, 40(a0)
    ld      t1, 48(a1)
    sd      t1, 48(a0)
    ld      t1, 56(a1)
    sd      t1, 56(a0)
    addi    a0, a0, 64
    addi    a1, a1, 64
    addi    a2, a2, -64
    j       .Lcopy_cache_line

.Lcopy_word:
    li      t0, 8
    bltu    a2, t0, .Lcopy_tail
    ld      t1, 0(a1)
    sd      t1, 0(a0)
    addi    a0, a0, 8
    addi    a1, a1, 8
    addi    a2, a2, -8
    j       .Lcopy_word

.Lcopy_tail:
    beqz    a2, .Lcopy_done
    lbu     t1, 0(a1)
    sb      t1, 0(a0)
    addi    a0, a0, 1
    addi    a1, a1, 1
    addi    a2, a2, -1
    j       .Lcopy_tail

.Lcopy_done:
    ret
.size __early_copy, .-__early_copy
