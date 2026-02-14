/* kernel.ld - 确保内核被加载到 OpenSBI 跳转后的地址 */
ENTRY(_start)
OUTPUT_ARCH(riscv)
BASE_ADDRESS = 0xffffffff80200000;
PHY_BASE_ADDRESS = 0x80200000;

SECTIONS
{
    . = BASE_ADDRESS;
    skernel = .;

    s_text = .;
    .text : AT(PHY_BASE_ADDRESS) {
        *(.text.entry)
        *(.text)
        *(.text.*)
        . = ALIGN(4K);
        e_text = .;
    }

    s_ivt = .;
    .ivt : {
        *(.ivt .ivt.*)
        . = ALIGN(4K);
        e_ivt = .;
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
        s_defer_pre = .;
        *(.defer.pre)
        e_defer_pre = .;

        . = ALIGN(4K);
        s_defer_post = .;
        *(.defer.post)
        e_defer_post = .;

        . = ALIGN(4K);
        e_rodata = .;
    }

    . = ALIGN(4K);
    s_data = .;
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
        *(.data.heap)
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

    . = ALIGN(4K);
    .comment : { *(.comment .comment.*) }
    .riscv.attributes : { *(.riscv.attributes .riscv.attributes.*) }

    . = ALIGN(2M);
    ekernel = .;
}