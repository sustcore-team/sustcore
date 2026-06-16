/* laboot.ld - 龙芯引导内核启动 */
ENTRY(_la_start)
OUTPUT_ARCH(loongarch64)

/* 内核虚拟地址偏移 */
KVA_OFFSET           = 0xffffffff00000000;

/* LA 基址 */
/* LA 引导启动程序 */
/* 与内核的其余部分无交集 */
/* 内核应当在启动后将这段内存回收 */
/* 实际上 [LABOOT_BASE, KERNEL_PAGING_BASE) 这段内存被 LABOOT 引导启动程序占用 */
/* 且可被内核回收用于动态内存分配 */
LABOOT_BASE             = 0x0000000000200000;
/* 内核页表基址 */
/* [KERNEL_PAGING_BASE, KERNEL_BASE) 这段内存被内核页表占用 */
/* 启动程序会将这段内存传递给内核以说明内核页表位置 */
/* 内核应在建立自己的页表后将这段内存回收 */
KERNEL_PAGING_BASE   = 0x00000000002E0000;
/* 内核物理地址基址 */
KERNEL_PHY_BASE      = 0x0000000000300000;

/* 内核虚拟地址基址与对应的物理地址基址 */
KERNEL_BASE          = KVA_OFFSET + KERNEL_PHY_BASE;

SECTIONS
{
    /* LABOOT 内存 */
    . = LABOOT_BASE;
    s_laboot = .;
    s_laboot_kva = s_laboot + KVA_OFFSET;

    .laboot : {
        KEEP(*(.laboot .laboot.*))
    }

    /* LABOOT 后初始化程序内存 */
    . += KVA_OFFSET;
    .laboot_post : AT(ADDR(.laboot_post) - KVA_OFFSET) {
        KEEP(*(.laboot_post .laboot_post.*))
    }

    /* LABOOT reclaimable 内存 */
    . = KERNEL_PAGING_BASE;
    s_laboot_reclaimable = .;
    s_laboot_reclaimable_kva = s_laboot_reclaimable + KVA_OFFSET;

    .laboot_reclaimable : AT(KERNEL_PAGING_BASE) {
        KEEP(*(.laboot_reclaimable .laboot_reclaimable.*))
    }
    . = KERNEL_PHY_BASE;
    e_laboot_reclaimable = .;
    e_laboot_reclaimable_kva = e_laboot_reclaimable + KVA_OFFSET;

    /* 内核内存 */
    . = KERNEL_BASE;
    skernel = .;

    s_text = .;
    .text : AT(KERNEL_PHY_BASE) {
        *(.text.entry)
        *(.text)
        *(.text.*)
        . = ALIGN(4K);
        e_text = .;
    }

    . = ALIGN(4K);
    s_rodata = .;
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)

        . = ALIGN(4K);
        s_initrd = .;
        *(.attach.initrd)
        e_initrd = .;

        . = ALIGN(4K);
        e_rodata = .;
    }

    . = ALIGN(4K);
    s_data = .;
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
        . = ALIGN(4K);
        e_data = .;
    }

    . = ALIGN(4K);
    s_bss = .;
    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        . = ALIGN(4K);
        e_bss = .;
    }

    . = ALIGN(4K);
    s_misc = .;

    . = ALIGN(2M);
    ekernel = .;
    ekernel_phys = ekernel - KVA_OFFSET;

    /DISCARD/ : {
        *(.comment)
        *(.riscv.attributes .riscv.attributes.*)
        *(.eh_frame*)
        *(.note .note.*)
    }
}
