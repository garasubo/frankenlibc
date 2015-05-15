ENTRY(_Reset)
SECTIONS
{
	. = 0x82000000;
	.startup . : { startup.o(.text) }
	.text : { *(.text) }
	.rodata :
	AT (LOADADDR(.text) + (ADDR(.rodata) - ADDR(.text)))
	{
		*(.rodata)
		__init_array_start = . ;
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		*(SORT_BY_INIT_PRIORITY(.ctors*))
		*(.init_array)
		__init_array_end = . ;
		__fini_array_start = . ;
		*(SORT_BY_INIT_PRIORITY(.fini_array.*))
		*(SORT_BY_INIT_PRIORITY(.dtors*))
		*(.fini_array)
		__fini_array_end = . ;
	}

	.data : { *(.data) }
	. = ALIGN(8);
	__bss_start = . ;
	.bss : { *(.bss COMMON) }
	. = ALIGN(8);
	__bss_end = . ;
	. = . + 0x10000; /* 64kB stack */
	stack_top = .;
}
