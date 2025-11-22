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

.section .data
.extern hart_id
.extern dtb_ptr

.section .text.entry, "ax"
.globl _start
_start:
    # 将64位寄存器a0, a1的值保存到全局变量中
    la t0, hart_id
    sd a0, 0(t0)
    la t1, dtb_ptr
    sd a1, 0(t1)
    # 设置栈指针
    la sp, boot_stack_top
    # 由于还未设置内核页表, 需要将sp转换为物理地址
    # li t2, 0Xffffffff00000000
    # sub sp, sp, t2
    # 跳转到 Rust/C 主函数
    call c_setup

.section .bss.stack
.align 12
boot_stack:
    .space 4096 * 4  # 16KB 栈空间
.globl boot_stack_top
boot_stack_top:
