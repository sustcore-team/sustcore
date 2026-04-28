/* default module linker script */
ENTRY(_start)
OUTPUT_ARCH(riscv)

BASE_ADDRESS = 0x80000000;

SECTIONS
{
	. = BASE_ADDRESS;

	. = ALIGN(4K);
	.text : {
		*(.text.entry)
		*(.text .text.*)
		*(.init)
		*(.fini)
	}

	. = ALIGN(4K);
	.rodata : {
		*(.rodata .rodata.*)
		*(.srodata .srodata.*)
	}

	. = ALIGN(4K);
	.data : {
		*(.data .data.*)
		*(.sdata .sdata.*)

		/* Required by crt0.S gp setup sequence. */
		. = ALIGN(8);
		PROVIDE(__global_pointer$ = . + 0x800);

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

		/* Backward compatibility for older toolchains using .ctors/.dtors */
		. = ALIGN(8);
		KEEP(*(.ctors .ctors.*))
		KEEP(*(.dtors .dtors.*))
	}

	. = ALIGN(4K);
	.bss (NOLOAD) : {
		*(.bss .bss.*)
		*(.sbss .sbss.*)
		*(COMMON)
	}

	. = ALIGN(4K);
	_end = .;

	/DISCARD/ : {
		*(.comment .comment.*)
		*(.note .note.*)
	}
}
