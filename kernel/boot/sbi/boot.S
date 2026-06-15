
.section .sbi_boot.text, "ax", @progbits

.extern _sbi_setup
.extern _sbi_post_start
.extern __sbi_reclaimable_cursor

.global _sbi_start
.type _sbi_start, @function
_sbi_start:
    // Disable all interrupts and paging
    csrw sie, zero
    csrw satp, zero
    sfence.vma zero, zero

    // 保存 OpenSBI 传入的启动参数, 跨 _sbi_setup 继续传递给 _sbi_post_start
    mv s0, a0
    mv s1, a1

    // write hart and fdt to global variables
    la t1, __sbi_boot_hart_id
    sd a0, 0(t1)
    la t1, __sbi_dtb_phys
    sd a1, 0(t1)

    // set sp to the top of the stack
    la sp, __sbi_boot_stack_top

    // jump to the setup function
    call _sbi_setup
_sbi_paging_setup:
    // 这里是一个从低地址到高地址的过渡阶段
    // 此处 a0 = satp 应设的值
    csrw satp, a0
    // 刷新 tlb
    sfence.vma zero, zero
_sbi_trampoline:
    mv a0, s0
    mv a1, s1
    la t1, __sbi_reclaimable_cursor
    ld a2, 0(t1)
    la ra, _sbi_post_start
    jr ra

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

// 给 sbi_boot 传递的启动变量
.section .sbi_boot.data, "aw", @progbits
.global __sbi_boot_hart_id
__sbi_boot_hart_id:
    .quad 0

.global __sbi_reclaimable_cursor
__sbi_reclaimable_cursor:
    .quad 0

.global __sbi_dtb_phys
__sbi_dtb_phys:
    .quad 0

// the sbi stack
.align 12
__sbi_boot_stack:
    .skip 8 * 1024
.global  __sbi_boot_stack_top
__sbi_boot_stack_top:
