/*
	Default linker script for MD407 (STM32F407)
	All code and data goes to RAM.
*/

/* Memory Spaces Definitions */
MEMORY
{
	RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 112K
}

SECTIONS
{
    /* the program code is stored in the .text section, which goes to RWM */
    .text :
    {
	    . = ALIGN(4);
	    
		*(.start_section)          /* startup code */
		*(.text)                   /* remaining code */
        *(.text.*)
        *(.rodata)                 /* read-only data (constants) */
        *(.rodata*)
	    *(.glue_7)
        *(.glue_7t)

	    . = ALIGN(4);
    } >RAM

    /* This is the initialized data section */
	.data  :
    {
	    . = ALIGN(4);
        
        *(.data)
        *(.data.*)

	    . = ALIGN(4);
    } >RAM

    /* This is the uninitialized data section */
    .bss :
    {
	    . = ALIGN(4);
        /* This is used by the startup in order to initialize the .bss secion */
        _sbss = .;
        
        *(.bss)
		*(.bss.*)
        *(COMMON)
        
	    . = ALIGN(4);
	    /* This is used by the startup in order to initialize the .bss secion */
		_ebss = . ;
    } >RAM
    
    PROVIDE ( end = _ebss );
    PROVIDE ( _end = _ebss );
}

