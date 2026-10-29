/* Boot menu overlay: loaded by src/payload/boot.c into overlay slot 0 before
 * TITLE.PRG. Zero-initialised data is kept in the image, since nothing
 * clears it. */
OUTPUT_FORMAT("elf32-littlemips")
ENTRY(rando_menuMain)

INCLUDE game.ld
INCLUDE payload_syms.ld /* generated from the resident payload */

MEMORY {
    slot0 (rwx) : ORIGIN = 0x80068800, LENGTH = 0x90000
}

SECTIONS {
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } > slot0
    .rodata : { *(.rodata .rodata.*) } > slot0
    .data : {
        *(.data .data.*) *(.sdata .sdata.*)
        *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON)
        . = ALIGN(4);
    } > slot0
    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.pdr) *(.gnu.attributes) }
}
