/* test.ld */
ENTRY(_start)
OUTPUT_ARCH(riscv)
BASE_ADDRESS = 0x82000000;

SECTIONS
{
    . = BASE_ADDRESS;
    s_text = .;
    .text : AT(BASE_ADDRESS) {
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