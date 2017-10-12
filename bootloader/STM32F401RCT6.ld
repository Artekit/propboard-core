MEMORY
{
	FLASH (rx)		: ORIGIN = 0x08000000, LENGTH = 16K
	RAM (xrw)		: ORIGIN = 0x20000000, LENGTH = 64K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
	.isr_vector :
	{
		program_offset = .;  

		. = ALIGN(4);
		KEEP(*(.isr_vector))
		. = ALIGN(8);
	} >FLASH
  
	.text :
	{
    	. = ALIGN(4);
    	*(.text*)
    	
    	KEEP (*(.init))
		KEEP (*(.fini))
    	   	
		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)
		 
		/* .dtors */
		*crtbegin.o(.dtors)
		*crtbegin?.o(.dtors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
		*(SORT(.dtors.*))
		*(.dtors)
		
		*(.rodata)
    	*(.rodata*)

    	*(.eh_frame*)

		. = ALIGN(8);
	} >FLASH

	.ARM.extab :
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} >FLASH
	
	.ARM : {
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
	} >FLASH

	.preinit_array :
	{
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array*))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	} >FLASH
	
	.init_array :
	{
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .);
	} >FLASH
	
	.fini_array :
	{
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array*))
		PROVIDE_HIDDEN (__fini_array_end = .);
	} >FLASH

	_sidata = LOADADDR(.data);

	.data : 
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data)
		*(.data*)

		. = ALIGN(8);
		_edata = .;
	} >RAM AT> FLASH
      
	. = ALIGN(4);
	.bss (NOLOAD) :
	{
		_sbss = .;
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)

		. = ALIGN(8);
		_ebss = .;         /* define a global symbol at bss end */
		__bss_end__ = _ebss;
	} >RAM

	_end = .;
	
	/DISCARD/ :
	{
		libc.a ( * )
		libm.a ( * )
		libgcc.a ( * )
	}
}
