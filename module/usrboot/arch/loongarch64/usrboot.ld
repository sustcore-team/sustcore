/**
 * @file usrboot.ld
 * @brief LoongArch64 usrboot 链接布局
 * @version 0.1.0-dev.1
 * @date 2026-08-11
 *
 * @copyright Copyright (c) 2026
 */

ENTRY(_start)
OUTPUT_ARCH(loongarch64)

BASE_ADDRESS = 0x000000200000;

PHDRS
{
	SEG_RX   PT_LOAD FLAGS(5); /* R-X */
	SEG_RO   PT_LOAD FLAGS(4); /* R-- */
	SEG_RW   PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS
{
	. = BASE_ADDRESS;

	. = ALIGN(4K);
	s_text = .;
	.text : {
		*(.text.entry)
		*(.text .text.*)
		*(.init)
		*(.fini)
	} :SEG_RX
	e_text = .;

	. = ALIGN(4K);
	s_rodata = .;
	.rodata : {
		*(.rodata .rodata.*)
		*(.srodata .srodata.*)
	} :SEG_RO
	e_rodata = .;

	. = ALIGN(4K);
	s_data = .;
	.data : {
		*(.data .data.*)
		*(.sdata .sdata.*)

		. = ALIGN(8);
		PROVIDE(__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE(__preinit_array_end = .);

		. = ALIGN(8);
		PROVIDE(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE(__init_array_end = .);

		. = ALIGN(8);
		PROVIDE(__fini_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE(__fini_array_end = .);

		KEEP(*(.ctors .ctors.*))
		KEEP(*(.dtors .dtors.*))
	} :SEG_RW
	e_data = .;

	. = ALIGN(4K);
	s_bss = .;
	.bss (NOLOAD) : {
		*(.bss .bss.*)
		*(.sbss .sbss.*)
		*(COMMON)
	} :SEG_RW
	e_bss = .;

	. = ALIGN(4K);
	_end = .;

	/DISCARD/ : {
		*(.comment .comment.*)
		*(.note .note.*)
	}
}
