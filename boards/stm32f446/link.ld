MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K /* 512K flash on ARM Cortex-M4 */
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K /* 128K RAM on ARM Cortex-M4 */
}

_estack = ORIGIN(RAM) + LENGTH(RAM); /* End of RAM, used as initial stack pointer */

SECTIONS
{

    .isr_vector :
    {
        KEEP(*(.isr_vector)) /* Keep the .isr_vector section */
    } > FLASH

    .text :
    {
        *(.text*) /* All .text sections from all object files */
    } > FLASH

    .rodata :
    {
        *(.rodata*) /* All .rodata sections from all object files */
    } > FLASH

    .data :
    {
        _sdata = .; /* Start of .data section in RAM */
        *(.data*) /* All .data sections from all object files */
        _edata = .; /* End of .data section in RAM */
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);  /* where .data lives in FLASH */


    .bss :
    {
        _sbss = .; /* Start of .bss section in RAM */
        *(.bss*) /* All .bss sections from all object files */
        *(COMMON) /* All COMMON sections from all object files */
        _ebss = .; /* End of .bss section in RAM */
    } > RAM

    /* Heap: place immediately after .bss; C code reads __heap_start/__heap_end */
    . = ALIGN(8);
    __heap_start = .;
    /* Reserve 4KB at top of RAM for MSP/interrupt stack to avoid heap/stack collision */
    . = _estack - 0x1000;
    __heap_end = .;
}