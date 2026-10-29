# Zenith repeat-kill drops: entry points patched into BATTLE.PRG's equipment
# drop rolls (see zenith.c). Each site is
#     lbu  v1, <dropRate>(sN)
#     jal  rando_dropStub<Kind>      (was nop)
#     <item pointer -> a2>           (was slt v0, v0, v1)
# with v0 holding the roll from vs_main_getRand(255). The stub tail-calls
# rando_dropRoll(roll, rate, item, kind), whose 0/1 result returns straight to
# the site in v0, as the slt it replaced would have left it. The drop
# functions save ra in their prologue, so the extra jal is safe.
    .set noreorder
    .text

    .globl rando_dropStubWeapon
rando_dropStubWeapon:
    move    $a0, $v0
    move    $a1, $v1
    j       rando_dropRoll
    li      $a3, 0

    .globl rando_dropStubShield
rando_dropStubShield:
    move    $a0, $v0
    move    $a1, $v1
    j       rando_dropRoll
    li      $a3, 1

    .globl rando_dropStubAccessory
rando_dropStubAccessory:
    move    $a0, $v0
    move    $a1, $v1
    j       rando_dropRoll
    li      $a3, 2

    .globl rando_dropStubArmor
rando_dropStubArmor:
    move    $a0, $v0
    move    $a1, $v1
    j       rando_dropRoll
    li      $a3, 3
