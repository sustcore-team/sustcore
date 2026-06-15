/* sbi.ld - OpenSBI 引导内核启动 */
ENTRY(_sbi_start)
OUTPUT_ARCH(riscv)

/* 内核虚拟地址偏移 */
KVA_OFFSET           = 0xffffffff00000000;

/* SBI 基址 */
/* SBI 引导启动程序 */
/* 与内核的其余部分无交集 */
/* 内核应当在启动后将这段内存回收 */
/* 实际上 [SBI_BASE, SBI_RECLAIMABLE_BASE) 这段内存被 SBI 引导启动程序占用 */
/* 且可被内核回收用于动态内存分配 */
SBI_BASE             = 0x0000000080200000;
/* SBI 可回收内存基址 */
/* [SBI_RECLAIMABLE_BASE, KERNEL_PHY_BASE) 这段内存被内核页表与 bootinfo 占用 */
/* 内核应在建立自己的页表并复制 bootinfo 后将这段内存回收 */
SBI_RECLAIMABLE_BASE = 0x00000000802E0000;
/* 内核物理地址基址 */
KERNEL_PHY_BASE      = 0x0000000080300000;

/* 内核虚拟地址基址与对应的物理地址基址 */
KERNEL_BASE          = KVA_OFFSET + KERNEL_PHY_BASE;

SECTIONS
{
    /* SBI 内存 */
    . = SBI_BASE;
    s_sbi = .;
    s_sbi_kva = s_sbi + KVA_OFFSET;

    .sbi_boot : {
        KEEP(*(.sbi_boot .sbi_boot.*))
    }

    /* SBI 后初始化程序内存 */
    . += KVA_OFFSET;
    .sbi_post : AT(ADDR(.sbi_post) - KVA_OFFSET) {
        KEEP(*(.sbi_post .sbi_post.*))
    }

    /* SBI 可回收内存 */
    . = SBI_RECLAIMABLE_BASE;
    s_sbi_reclaimable = .;
    s_sbi_reclaimable_kva = s_sbi_reclaimable + KVA_OFFSET;

    .sbi_reclaimable : AT(SBI_RECLAIMABLE_BASE) {
        KEEP(*(.sbi_reclaimable .sbi_reclaimable.*))
    }
    . = KERNEL_PHY_BASE;
    e_sbi_reclaimable = .;
    e_sbi_reclaimable_kva = e_sbi_reclaimable + KVA_OFFSET;

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
