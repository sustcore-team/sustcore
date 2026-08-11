/**
 * @file early_copy.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 启动期内存复制原语
 * @version 0.1.0-dev.1
 * @date 2026-08-04
 *
 * @copyright Copyright (c) 2026
 *
 */

.section .init.text, "ax", @progbits
.p2align 4
.global __early_copy
.type __early_copy, @function
__early_copy:
    beqz    $a2, .Lcopy_done

    # 只有源和目标具有相同对齐关系时，双字访问才会同时对齐。
    xor     $t0, $a0, $a1
    andi    $t0, $t0, 7
    bnez    $t0, .Lcopy_tail

.Lcopy_align:
    andi    $t0, $a0, 7
    beqz    $t0, .Lcopy_cache_line
    ld.bu   $t1, $a1, 0
    st.b    $t1, $a0, 0
    addi.d  $a0, $a0, 1
    addi.d  $a1, $a1, 1
    addi.d  $a2, $a2, -1
    bnez    $a2, .Lcopy_align
    ret

.Lcopy_cache_line:
    addi.d  $t0, $zero, 64
    bltu    $a2, $t0, .Lcopy_word
    ld.d    $t1, $a1, 0
    st.d    $t1, $a0, 0
    ld.d    $t1, $a1, 8
    st.d    $t1, $a0, 8
    ld.d    $t1, $a1, 16
    st.d    $t1, $a0, 16
    ld.d    $t1, $a1, 24
    st.d    $t1, $a0, 24
    ld.d    $t1, $a1, 32
    st.d    $t1, $a0, 32
    ld.d    $t1, $a1, 40
    st.d    $t1, $a0, 40
    ld.d    $t1, $a1, 48
    st.d    $t1, $a0, 48
    ld.d    $t1, $a1, 56
    st.d    $t1, $a0, 56
    addi.d  $a0, $a0, 64
    addi.d  $a1, $a1, 64
    addi.d  $a2, $a2, -64
    b       .Lcopy_cache_line

.Lcopy_word:
    addi.d  $t0, $zero, 8
    bltu    $a2, $t0, .Lcopy_tail
    ld.d    $t1, $a1, 0
    st.d    $t1, $a0, 0
    addi.d  $a0, $a0, 8
    addi.d  $a1, $a1, 8
    addi.d  $a2, $a2, -8
    b       .Lcopy_word

.Lcopy_tail:
    beqz    $a2, .Lcopy_done
    ld.bu   $t1, $a1, 0
    st.b    $t1, $a0, 0
    addi.d  $a0, $a0, 1
    addi.d  $a1, $a1, 1
    addi.d  $a2, $a2, -1
    b       .Lcopy_tail

.Lcopy_done:
    ret
.size __early_copy, .-__early_copy
