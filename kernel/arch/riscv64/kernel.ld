/* kernel.ld - 确保内核被加载到 OpenSBI 跳转后的地址 */
ENTRY(_start)
SECTIONS
{
    . = 0x80200000;
    skernel = .;

    s_text = .;
    .text : {
        *(.text.entry)
        *(.text .text.*)
        . = ALIGN(4K);
        e_test = .;
    }
    
    . = ALIGN(4K);
    s_rodata = .;
    .rodata : { *(.rodata .rodata.*) }

    . = ALIGN(4K);
    e_rodata = .;
    s_data = .;
    .data : { *(.data .data.ivt) *(.data .data.*) *(.data .data.heap) }

    . = ALIGN(4K);
    s_sdata = .;
    .sdata : { *(.sdata .sdata.*) }

    . = ALIGN(4K);
    e_sdata = .;
    s_bss = .;
    .bss : { *(.bss .bss.*) }

    . = ALIGN(4K);
    e_bss = .;
    
    .comment : { *(.comment .comment.*) }
    .riscv.attributes : { *(.riscv.attributes .riscv.attributes.*) }

    ekernel = .;
}