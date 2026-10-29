/* Shared item pools. */
#include "rando.h"

/* Grimoire ids with real names; the range has "untitled" holes. */
static const u16 grimoires[] = {
    0x182, 0x183, 0x184, 0x185, 0x186, 0x187, 0x188, 0x189, 0x18A, 0x18B,
    0x18F, 0x193, 0x197, 0x19B, 0x19F, 0x1A3,
    0x1A7, 0x1A8, 0x1A9, 0x1AA, 0x1AB, 0x1AC, 0x1AD, 0x1AE, 0x1AF, 0x1B0,
    0x1B1, 0x1B2, 0x1B3, 0x1B4, 0x1B5, 0x1B6, 0x1B7, 0x1B8, 0x1B9, 0x1BA,
    0x1BB, 0x1BC, 0x1BD, 0x1BE, 0x1BF, 0x1C0, 0x1C1, 0x1C2, 0x1C3, 0x1C4,
    0x1C5, 0x1C6, 0x1C7, 0x1C8,
};

#define CONSUMABLE_COUNT ITEM_CONSUMABLE_COUNT
#define GRIMOIRE_COUNT (sizeof(grimoires) / sizeof(grimoires[0]))

const int item_grimoireCount = GRIMOIRE_COUNT;

int item_isConsumable(int id)
{
    return id >= ITEM_CONSUMABLE_FIRST && id <= ITEM_CONSUMABLE_LAST;
}

int item_isGrimoire(int id)
{
    return id >= ITEM_GRIMOIRE_FIRST && id <= ITEM_GRIMOIRE_LAST;
}

int item_isProgression(int id)
{
    return id >= ITEM_KEY_FIRST && id <= ITEM_SIGIL_LAST;
}

u16 item_randomConsumable(rng_t* rng)
{
    return rng_range(rng, ITEM_CONSUMABLE_FIRST, ITEM_CONSUMABLE_LAST);
}

u16 item_grimoireAt(int i)
{
    return grimoires[i];
}

u16 item_randomGrimoire(rng_t* rng)
{
    return grimoires[rng_range(rng, 0, GRIMOIRE_COUNT - 1)];
}

/* Any non-progression misc item, consumables and grimoires weighted by how
 * many of each exist. */
u16 item_randomMisc(rng_t* rng)
{
    int pick = rng_range(rng, 0, CONSUMABLE_COUNT + GRIMOIRE_COUNT - 1);

    if (pick < CONSUMABLE_COUNT) {
        return ITEM_CONSUMABLE_FIRST + pick;
    }
    return grimoires[pick - CONSUMABLE_COUNT];
}
