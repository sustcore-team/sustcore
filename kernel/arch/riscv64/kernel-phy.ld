/* kernel-phy.ld - 调试用, 在内核进入高地址段前发挥作用 */
ENTRY(_start)
OUTPUT_ARCH(riscv)
BASE_ADDRESS = 0x80200000;
PHY_BASE_ADDRESS = 0x80200000;

SECTIONS
{
    . = BASE_ADDRESS;
    skernel = .;

    s_text = .;
    .text : AT(PHY_BASE_ADDRESS) {
        *(.text.entry)
        *(.text .text.*)
        . = ALIGN(4K);
        e_text = .;
    }

    . = ALIGN(4K);
    s_rodata = .;
    .rodata : { *(.rodata .rodata.*) }

    . = ALIGN(4K);
    e_rodata = .;
    s_data = .;
    .data : { *(.data .data.ivt) *(.data .data.*) *(.data .data.heap) }

    . = ALIGN(4K);
    e_data = .;
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

    . = ALIGN(2M);
    ekernel = .;
}