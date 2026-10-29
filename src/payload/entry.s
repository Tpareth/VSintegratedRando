# Exe entry point. The BIOS jumps here instead of the game's own entry: clear
# our bss, then continue into the original startup code.
    .set noreorder
    .section .text.entry, "ax"
    .globl rando_start
rando_start:
    la      $t0, __bss_start
    la      $t1, __bss_end
1:  sltu    $t2, $t0, $t1
    beqz    $t2, 2f
    nop
    sw      $zero, 0($t0)
    b       1b
    addiu   $t0, $t0, 4
2:  j       GAME_ENTRY
    nop
