/* kernel.ld - 确保内核被加载到 OpenSBI 跳转后的地址 */
ENTRY(_start)
OUTPUT_ARCH(riscv)
BASE_ADDRESS = 0x82000000;

SECTIONS
{
    s_text = .;
    .text : AT(BASE_ADDRESS) {
        *(.text.entry)
        *(.text)
        *(.text.*)
        . = ALIGN(4K);
        e_text = .;
    }

    . = ALIGN(4K);
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
}