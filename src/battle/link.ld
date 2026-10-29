/* Battle overlay: loaded by src/payload/hooks.c (rando_onAshleyInit) into the
 * free RAM after BATTLE.PRG, whole sectors at a time, so it must fit in 7
 * sectors before overlay slot 1 (0x800F9800). It starts at 0x800F6000, not
 * at BATTLE.PRG's vanilla end (0x800F5924): the Rood Reverse QoL mod and
 * Zenith both append code to BATTLE.PRG up to there. Zero-initialised data is
 * kept in the image, since nothing clears it. */
OUTPUT_FORMAT("elf32-littlemips")
ENTRY(rando_overlayHeader)

INCLUDE game.ld
INCLUDE payload_syms.ld /* generated from the resident payload */

MEMORY {
    gap (rwx) : ORIGIN = 0x800F6000, LENGTH = 7 * 0x800
}

SECTIONS {
    .header : { KEEP(*(.header)) } > gap
    .text : { *(.text .text.*) } > gap
    .rodata : { *(.rodata .rodata.*) } > gap
    .data : {
        *(.data .data.*) *(.sdata .sdata.*)
        *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON)
        . = ALIGN(4);
    } > gap
    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.pdr) *(.gnu.attributes) }
}
