/* sections.ld - common section layout (included by every image's linker script after MEMORY). */
SECTIONS
{
    .isr_vector : { . = ALIGN(512); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH

    .text : {
        . = ALIGN(4);
        *(.text*) *(.rodata*) *(.glue_7) *(.glue_7t)
        KEEP(*(.init)) KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH

    _sidata = LOADADDR(.data);
    .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH
    .bss  : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM

    /* The main (MSP/ISR) stack lives at the top of RAM and grows down; this assertion guarantees
     * that statics leave at least _Min_Stack_Size bytes for it. */
    ._stack_reserve (NOLOAD) : { . = ALIGN(8); . = . + _Min_Stack_Size; } > RAM

    /DISCARD/ : { *(.note*) *(.comment*) }
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
