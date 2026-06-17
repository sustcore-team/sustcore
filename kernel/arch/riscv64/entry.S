/*
 * Sustcore
 * 
 * 作者: theflysong
 * 
 * entry.S
 * 
 * 在此处键入描述
 * 
 */

#include "hart.S"

.section .data
.extern dtb_ptr
.extern bootinfo_ptr
.extern __hart_context

.extern c_setup

.section .text.entry, "ax"
.globl _start
_start:
    # 根据 hart id 计算当前 hart 的上下文槽地址并保存到 tp
    li t0, MAX_HARTS
    bgeu a0, t0, .Linvalid_hart
    # tp = __hart_context + hart_id * sizeof(HartContext)
    locate_hart_context a0, t1
    # tp.hart_id = a0
    set_hart_id a0
    # 设置 bootinfo_ptr
    la t1, bootinfo_ptr
    sd a1, 0(t1)
    # 设置栈指针
    la sp, boot_stack_top
    # 跳转到 Rust/C 主函数
    call c_setup

.Linvalid_hart:
    wfi
    j .Linvalid_hart

.section .text, "ax"
.globl redive
redive:
    # 设置栈指针
    la sp, boot_stack_top
    wfi
    j redive

.section .bss.stack
.align 12
boot_stack:
    .space 1024 * 64  # 64KB 栈空间
.globl boot_stack_top
boot_stack_top:
