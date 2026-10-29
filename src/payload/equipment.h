/* Equipment pools generated from the disc by tools/gen_equipment.py:
 * inventory-format records (vs_blade, vs_grip, vs_armor for shields and
 * armor, vs_gem) with the per-slot
 * bookkeeping fields zeroed. Kept as byte arrays so the generated file does
 * not depend on struct layouts. */
#pragma once
#include "vs.h"

extern const u8 pool_blades[][0x2C];
extern const int pool_bladeCount;
extern const u8 pool_grips[][0x10];
extern const int pool_gripCount;
extern const u8 pool_shields[][0x28];
extern const int pool_shieldCount;
extern const u8 pool_armor[][0x28];
extern const int pool_armorCount;
extern const u8 pool_gems[][0x1C];
extern const int pool_gemCount;
