/* sbi.ld - OpenSBI 引导内核启动 */
ENTRY(_sbi_start)
OUTPUT_ARCH(riscv)

/* 内核虚拟地址偏移 */
KVA_START           = 0xffffffff00000000;

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
KERNEL_BASE          = KVA_START + KERNEL_PHY_BASE;

SECTIONS
{
    /* SBI 内存 */
    . = SBI_BASE;
    s_sbi = .;
    s_sbi_kva = s_sbi + KVA_START;

    .sbi_boot.text : {
        KEEP(*(.sbi_boot.text .sbi_boot.text.*))
    }

    .sbi_boot.rodata : {
        KEEP(*(.sbi_boot.rodata .sbi_boot.rodata.*))
    }

    . = ALIGN(8);
    .sbi_boot.data : {
        KEEP(*(.sbi_boot.data .sbi_boot.data.*))
        . = ALIGN(8);
        __sbi_post_entry_ptr = .;
        QUAD(ABSOLUTE(_sbi_post_start))
    }

    .sbi_boot.bss (NOLOAD) : {
        KEEP(*(.sbi_boot.bss .sbi_boot.bss.*))
    }

    /* SBI 后初始化程序内存 */
    . += KVA_START;
    .sbi_post_boot.text : AT(ADDR(.sbi_post_boot.text) - KVA_START) {
        KEEP(*(.sbi_post_boot.text .sbi_post_boot.text.*))
    }

    .sbi_post_boot.rodata : AT(ADDR(.sbi_post_boot.rodata) - KVA_START) {
        KEEP(*(.sbi_post_boot.rodata .sbi_post_boot.rodata.*))
    }

    .sbi_post_boot.data : AT(ADDR(.sbi_post_boot.data) - KVA_START) {
        KEEP(*(.sbi_post_boot.data .sbi_post_boot.data.*))
    }

    .sbi_post_boot.bss (NOLOAD) : AT(ADDR(.sbi_post_boot.bss) - KVA_START) {
        KEEP(*(.sbi_post_boot.bss .sbi_post_boot.bss.*))
    }

    /* SBI 可回收内存 */
    . = SBI_RECLAIMABLE_BASE;
    s_sbi_reclaimable = .;
    s_sbi_reclaimable_kva = s_sbi_reclaimable + KVA_START;

    .sbi_reclaimable : AT(SBI_RECLAIMABLE_BASE) {
        KEEP(*(.sbi_reclaimable .sbi_reclaimable.*))
    }
    . = KERNEL_PHY_BASE;
    e_sbi_reclaimable = .;
    e_sbi_reclaimable_kva = e_sbi_reclaimable + KVA_START;

    /* 内核内存 */
    . = KERNEL_BASE;
    skernel = .;

    /* 初始化段统一放在内核永久代码之前，便于启动后整体回收。 */
    s_init = .;
    s_init_phys = s_init - KVA_START;

    s_init_text = .;
    .init.text : AT(KERNEL_PHY_BASE) {
        *(.init.text .init.text.*)
        . = ALIGN(4K);
        e_init_text = .;
    }

    s_init_rodata = .;
    .init.rodata : {
        *(.init.rodata .init.rodata.*)
        . = ALIGN(8);
        PROVIDE(__preinit_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE(__preinit_array_end = .);
        . = ALIGN(8);
        PROVIDE(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)
               SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array
               EXCLUDE_FILE(*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o)
               .ctors))
        PROVIDE(__init_array_end = .);
        . = ALIGN(4K);
        e_init_rodata = .;
    }

    s_init_data = .;
    .init.data : {
        *(.init.data .init.data.*)
        . = ALIGN(4K);
        e_init_data = .;
    }

    s_init_bss = .;
    .init.bss (NOLOAD) : {
        *(.init.bss .init.bss.*)
        . = ALIGN(4K);
        e_init_bss = .;
    }

    e_init = .;

    s_text = .;
    .text : {
        . = ALIGN(4K);
        KEEP(*(.text.smp_entry .text.smp_entry.*))
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
        *(.got .got.*)
        . = ALIGN(4K);
        e_data = .;
    }

    /* 当前栈正在使用时不能被 BSP 的 BSS 清零过程覆盖. */
    . = ALIGN(4K);
    .bss.stack (NOLOAD) : {
        __bsp_stack_bottom = .;
        . += 64K;
        __bsp_stack_top = .;
    }

    . = ALIGN(4K);
    s_bss = .;
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        . = ALIGN(4K);
        e_bss = .;
    }

    . = ALIGN(2M);
    ekernel = .;
    ekernel_phys = ekernel - KVA_START;

    /DISCARD/ : {
        *(.comment)
        *(.riscv.attributes .riscv.attributes.*)
        *(.eh_frame*)
        *(.note .note.*)
    }

    ASSERT((__bsp_stack_top & 0xF) == 0,
           "BSP stack must be 16-byte aligned")
    ASSERT(e_bss >= s_bss, "invalid kernel BSS range")
    ASSERT((s_init & 0xFFF) == 0, "init section must be page aligned")
    ASSERT((e_init & 0xFFF) == 0, "init section end must be page aligned")
    ASSERT(s_text == e_init, "permanent text must immediately follow init")
    ASSERT((sbi_secondary_trampoline & 0xFFF) == 0,
           "SBI AP trampoline must be page aligned")
}
