/**
 * @file early_clear.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 启动期内存清零原语
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

.section .init.text.early_clear, "ax", @progbits
.p2align 4
.global __early_clear
.type __early_clear, @function
__early_clear:
    bgeu    a0, a1, .Lclear_done

.Lclear_align:
    # 先逐字节对齐目标地址，后续才能安全使用 64 位批量写入。
    andi    t0, a0, 7
    beqz    t0, .Lclear_cache_line
    sb      zero, 0(a0)
    addi    a0, a0, 1
    bltu    a0, a1, .Lclear_align
    ret

.Lclear_cache_line:
    # 完整缓存行按八个双字展开，减少启动期大范围清零的循环开销。
    sub     t0, a1, a0
    li      t1, 64
    bltu    t0, t1, .Lclear_word
    sd      zero, 0(a0)
    sd      zero, 8(a0)
    sd      zero, 16(a0)
    sd      zero, 24(a0)
    sd      zero, 32(a0)
    sd      zero, 40(a0)
    sd      zero, 48(a0)
    sd      zero, 56(a0)
    addi    a0, a0, 64
    j       .Lclear_cache_line

.Lclear_word:
    li      t1, 8
    bltu    t0, t1, .Lclear_tail
    sd      zero, 0(a0)
    addi    a0, a0, 8
    sub     t0, a1, a0
    j       .Lclear_word

.Lclear_tail:
    # 不足一个双字的末尾仍按字节处理，保证不越过调用者给出的上界。
    beqz    t0, .Lclear_done
    sb      zero, 0(a0)
    addi    a0, a0, 1
    addi    t0, t0, -1
    j       .Lclear_tail

.Lclear_done:
    ret
.size __early_clear, .-__early_clear
