ENTRY(_start)

SECTIONS
{
    .text :
    {
        KEEP(*(.entry))
        *(.ltext)
        *(.text*)
        *(.rodata*)
        *(.lrodata*)
        *(.srodata*)
        *(.data*)
        . = ALIGN(8);
        *(.bss*)
        *(.lbss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(8);
    }
    /DISCARD/ : { *(.comment) *(.note) *(.eh_frame*) }
}
