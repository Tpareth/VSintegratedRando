/* Resident payload: appended to SLUS_010.40 and loaded by the BIOS into the
 * unused RAM between the end of the exe and overlay slot 0. */
OUTPUT_FORMAT("elf32-littlemips")
ENTRY(rando_start)

INCLUDE game.ld

MEMORY {
    resident (rwx) : ORIGIN = 0x80062000, LENGTH = 0x6800
}

SECTIONS {
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } > resident
    .rodata : { *(.rodata .rodata.*) } > resident
    .data : { *(.data .data.*) *(.sdata .sdata.*) } > resident
    . = ALIGN(4);
    __payload_file_end = .;
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > resident
    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.pdr) *(.gnu.attributes) }
}
