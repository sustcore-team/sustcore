/* laboot.ld - 龙芯引导内核启动 */
ENTRY(_la_start)
OUTPUT_ARCH(loongarch64)

/* 内核虚拟地址偏移 */
KVA_START           = 0xffffffff00000000;

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
KERNEL_BASE          = KVA_START + KERNEL_PHY_BASE;

SECTIONS
{
    /* LABOOT 内存 */
    . = LABOOT_BASE;
    s_laboot = .;
    s_laboot_kva = s_laboot + KVA_START;

    .laboot.text : {
        KEEP(*(.laboot.text .laboot.text.*))
    }

    .laboot.rodata : {
        KEEP(*(.laboot.rodata .laboot.rodata.*))
    }

    .laboot.data : {
        KEEP(*(.laboot.data .laboot.data.*))
    }

    .laboot.bss (NOLOAD) : {
        KEEP(*(.laboot.bss .laboot.bss.*))
    }

    /* LABOOT 后初始化程序内存 */
    . += KVA_START;
    .laboot_post.text : AT(ADDR(.laboot_post.text) - KVA_START) {
        KEEP(*(.laboot_post.text .laboot_post.text.*))
    }

    .laboot_post.rodata : AT(ADDR(.laboot_post.rodata) - KVA_START) {
        KEEP(*(.laboot_post.rodata .laboot_post.rodata.*))
    }

    .laboot_post.data : AT(ADDR(.laboot_post.data) - KVA_START) {
        KEEP(*(.laboot_post.data .laboot_post.data.*))
    }

    .laboot_post.bss (NOLOAD) : AT(ADDR(.laboot_post.bss) - KVA_START) {
        KEEP(*(.laboot_post.bss .laboot_post.bss.*))
    }

    /* LABOOT reclaimable 内存 */
    . = KERNEL_PAGING_BASE;
    s_laboot_reclaimable = .;
    s_laboot_reclaimable_kva = s_laboot_reclaimable + KVA_START;

    .laboot_reclaimable : AT(KERNEL_PAGING_BASE) {
        KEEP(*(.laboot_reclaimable .laboot_reclaimable.*))
    }
    . = KERNEL_PHY_BASE;
    e_laboot_reclaimable = .;
    e_laboot_reclaimable_kva = e_laboot_reclaimable + KVA_START;

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
        . = ALIGN(16);
        s_usrboot = .;
        KEEP(*(.rodata.usrboot))
        KEEP(*(.rodata.usrboot.*))
        e_usrboot = .;

        *(.rodata .rodata.*)
        *(.srodata .srodata.*)

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
    ASSERT((laboot_secondary_trampoline & 0xFFF) == 0,
           "LABOOT AP trampoline must be page aligned")
}
