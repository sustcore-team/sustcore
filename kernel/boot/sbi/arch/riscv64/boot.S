/**
 * @file boot.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI RISC-V 内核汇编入口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */


.section .sbi_boot.text, "ax", @progbits

.extern _sbi_setup
.extern __sbi_post_entry_ptr
.extern __sbi_reclaimable_cursor

.global _sbi_start
.type _sbi_start, @function
_sbi_start:
    // 先关闭中断和分页，确保后续低地址启动代码在确定的地址空间中运行。
    csrw sie, zero
    csrw satp, zero
    sfence.vma zero, zero

    // 保存 OpenSBI 传入的启动参数, 跨 _sbi_setup 继续传递给 _sbi_post_start
    mv s0, a0
    mv s1, a1

    // 将 hart 与 FDT 物理地址保存到启动期全局变量。
    la t1, __sbi_boot_hart_id
    sd a0, 0(t1)
    la t1, __sbi_dtb_phys
    sd a1, 0(t1)

    // 切换到链接脚本预留的启动栈后再进入 C++。
    la sp, __sbi_boot_stack_top

    call _sbi_setup
_sbi_paging_setup:
    // _sbi_setup 返回完整 SATP；新页表同时保留当前低地址和目标高地址映射。
    csrw satp, a0
    sfence.vma zero, zero
_sbi_trampoline:
    // 页表生效后恢复固件参数，并跳往已经映射的高半区入口。
    mv a0, s0
    mv a1, s1
    la t1, __sbi_reclaimable_cursor
    ld a2, 0(t1)
    la t1, __sbi_post_entry_ptr
    ld t1, 0(t1)
    jr t1

// _sbi_write
.global _sbi_write
.type _sbi_write, @function
_sbi_write:
    // a7 : eid = SBI_EID_DBCN = 0x4442434E
    // a6 : fid = SBI_CONSOLE_WRITE = 0
    // a0 : length
    // a1 : buffer
    // a2 : addr_hi = 0
    li a7, 0x4442434E
    li a6, 0
    li a2, 0
    ecall
    ret

// 给 sbi_boot 传递的启动变量和启动期专用栈。
.section .sbi_boot.bss, "aw", @nobits
.global __sbi_boot_hart_id
__sbi_boot_hart_id:
    .quad 0

.global __sbi_reclaimable_cursor
__sbi_reclaimable_cursor:
    .quad 0

.global __sbi_dtb_phys
__sbi_dtb_phys:
    .quad 0

.align 12
__sbi_boot_stack:
    .skip 8 * 1024
.global  __sbi_boot_stack_top
__sbi_boot_stack_top:
