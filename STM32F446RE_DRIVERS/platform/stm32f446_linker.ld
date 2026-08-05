/* Entry Point */
ENTRY(Reset_Handler)

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size 	= 0x200;      	/* required amount of heap  */
_Min_Stack_Size = 0x400; 		/* required amount of stack */

MEMORY
{
	flash(rx) : ORIGIN = 0x08000000, LENGTH = 256K
	sram(rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Reserve 2 KB for stack (adjust as needed) */
_stack_size 	= 0x800;  /* 2 KB - safe default */
_stack_top 		= ORIGIN(sram) + LENGTH(sram);
_estack			= _stack_top;
_stack_bottom 	= _stack_top - _stack_size;

SECTIONS
{
	/* The startup code goes first into FLASH */
	.isr_vector :
	{
		. = ALIGN(4);
		_vectors_start = .;
		KEEP(*(.isr_vector))	/* Startup code */
		. = ALIGN(4);
		_vectors_end = .;
	} > flash

	/* RAM vector table (relocation) */
	.ram_vector (NOLOAD) :
	{
		. = ALIGN(128);
		_ram_vector_start = .;
		. = . + (16 + 91) * 4;  /*core + device IRQs */
		_ram_vector_end = .;
	} > sram

	.text :
	{
		. = ALIGN(4);
		*(.text)					/* All code */
		*(.text.*)					/* Code in sections like .text.startup */
		*(.glue_7)					/* Glue arm to thumb code */
		*(.glue_7t)					/* Glue thumb to arm code */
		*(.eh_frame)				/* Exception handling and stack unwinding */

		KEEP(*(.init))				/* Code that runs before main */
		KEEP(*(.fini))				/* Code that runs after main */
		. = ALIGN(4);
		_etext = .;					/* End address of code (used by some tools) */
	} > flash

	.button_list :
	{
		. = ALIGN(4);
		__button_list_start = .;
		KEEP(*(.button_list))
		__button_list_end = .;
		. = ALIGN(4);
	} > flash

	.rodata :						/*Constants, string literals, const variables go here.*/
	{
		. = ALIGN(4);
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(4);
	} > flash
	
	
	.ARM.exidx :
	{
		. = ALIGN(4);
		*(.ARM.exidx*)
		. = ALIGN(4);
	} > flash

	/*If you ever use exceptions (even accidentally), Otherwise stack unwinding can break*/
	.ARM.extab :
	{
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
	} >flash

	.ARM :
	{
		. = ALIGN(4);
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
		. = ALIGN(4);
	} >flash

	/*C++ Support*/
	.preinit_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN(__preinit_array_start = .);
		KEEP(*(.preinit_array*))
		PROVIDE_HIDDEN(__preinit_array_end = .);
		. = ALIGN(4);
	} > flash

	.init_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array*))
		PROVIDE_HIDDEN(__init_array_end = .);
		. = ALIGN(4);
	} > flash

	.fini_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN(_fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array*))
		PROVIDE_HIDDEN(__fini_array_end = .);
		. = ALIGN(4);
	} > flash

	.data :							/* Initialized global/static variables */
	{
		. = ALIGN(4);
		_sdata = .;					/* Start of .data in RAM */
		*(.data)
		*(.data.*)

		
    	*(.clock)					/* Including .clock in .data so it gets copied */

		*(.RamFunc)        			/* .RamFunc sections */
    	*(.RamFunc*)       			/* .RamFunc* sections */

		. =  ALIGN(4);
		_edata = .;					/* End of .data in RAM */
	} > sram AT > flash				/* Runtime in RAM, loaded from Flash */

	_sidata = LOADADDR(.data);		/* Flash address where .data image is stored */

	.bss(NOLOAD) :							/* Zero-initialized variables */
	{
		. = ALIGN(4);
		_sbss = .;					/* Start of .bss */
		*(.bss)
		*(.bss.*)
		*(COMMON)					/* C global variables without initializer */
		. = ALIGN(4);
		_ebss = .;					/* End of .bss */
	} > sram


		/DISCARD/ :
	{
		*(.note*)
		*(.comment*)

		/* Remove information from the standard libraries
		libc.a ( * )
		libm.a ( * )
		libgcc.a ( * )*/
	}

	/*assert that we haven't overflowed flash */
	__flash_end = ORIGIN(flash) + LENGTH(flash);
	ASSERT(LOADADDR(.data) + SIZEOF(.data) <= __flash_end,
		"Flash overflow: .ccm_text section exceeds Flash boundary")
}
