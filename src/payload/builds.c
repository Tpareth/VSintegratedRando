/* Ashley builds (port of ChaoticBrave's Ashley::buildIterate): a starting
 * setup around one weapon type, with matching stats, blade, grip and items.
 * Differences from the original: the blade and grip are the real records of
 * the chosen items rather than the Fandango's with some fields replaced, and
 * two slips are fixed (the Dagger's fifth item ignored its grimoire roll, and
 * one Staff grimoire choice was written to the wrong variable). */
#include "rando.h"

#define ANY_GRIMOIRE 0xFF
#define SIGIL_NOT_CHAMOMILE 0xFE /* any sigil except the Chamomile Sigil */
#define BRONZE_OR_SIGIL 0xFD /* Bronze Key, or SIGIL_NOT_CHAMOMILE */

#define BRONZE_KEY 0x1CA
#define CHAMOMILE_SIGIL 0x1D2

/* Item choices for one inventory slot, as item id - 0x100 (ids 0x143-0x1E9),
 * zero-terminated; one is picked at random. */
typedef u8 itemChoice_t[12];

typedef struct {
    u8 bladeId;
    u8 gripId;
    /* Lower bounds of 11-wide ranges (e.g. 185 means 185-195). */
    s16 hp, mp;
    u8 strength, intelligence, agility;
    itemChoice_t items[5];
} build_t;

static const build_t builds[BUILD_COUNT] = {
    [BUILD_DAGGER - 1] = { 2, 96, 185, 15, 85, 55, 105,
        { { 89 }, { 92 }, { 100 }, { BRONZE_OR_SIGIL },
            { 136, 138, 163, 167, 168, 179, 180, 185, 188, 190, 191 } } },
    [BUILD_GREAT_SWORD - 1] = { 26, 96, 320, 30, 105, 75, 95,
        { { 90 }, { 93 }, { 95, 96, 97, 98, 99 }, { 135, 137, 159, 169 }, { 175, 181 } } },
    [BUILD_AXE - 1] = { 36, 105, 220, 45, 100, 85, 95,
        { { 88 }, { 89 }, { 95 }, { 175 }, { 181 } } },
    [BUILD_MACE - 1] = { 44, 105, 155, 30, 65, 75, 95,
        { { 80 }, { 87 }, { 90, 91, 92, 93, 94 }, { 139, 159, 168, 169, 173 },
            { 177, 181, 184, 185, 186, 187, 189 } } },
    [BUILD_GREAT_AXE - 1] = { 52, 105, 235, 45, 95, 90, 90,
        { { 78 }, { 93 }, { 98 }, { 136, 138, 163, 167, 168 }, { 176, 178, 180, 182, 185 } } },
    [BUILD_GREAT_MACE - 1] = { 64, 105, 235, 50, 100, 90, 95,
        { { 88 }, { 90 }, { 92 }, { 176, 178, 180, 182, 183, 184, 185, 186, 187 }, { 188, 189, 190, 191 } } },
    [BUILD_POLEARM - 1] = { 71, 115, 235, 15, 105, 45, 95,
        { { 78 }, { 87, 89 }, { 90, 92 }, { 173 }, { 181 } } },
    [BUILD_STAFF - 1] = { 58, 105, 145, 55, 50, 145, 95,
        { { 74 }, { 87 }, { 91, 94 }, { SIGIL_NOT_CHAMOMILE }, { ANY_GRIMOIRE } } },
    [BUILD_CROSSBOW - 1] = { 84, 121, 195, 25, 95, 75, 95,
        { { 78 }, { 85 }, { 92 }, { 131, 143, 193, 197 }, { 173, 180, 188, 191, 192 } } },
};

enum { STREAM_PART_BUILD = 0x10, STREAM_PART_BUILD_STATS, STREAM_PART_BUILD_GEAR };

/* The chosen build (1..BUILD_COUNT), with Random resolved from the seed, or 0. */
int rando_build(void)
{
    int build = (rando_config.options & OPT_BUILD_MASK) >> OPT_BUILD_SHIFT;
    rng_t rng;

    if (build == BUILD_RANDOM) {
        rng_init(&rng, rando_config.seed, STREAM_ASHLEY | STREAM_PART_BUILD);
        build = rng_range(&rng, 1, BUILD_COUNT);
    }
    return build >= 1 && build <= BUILD_COUNT ? build : 0;
}

void rando_buildStats(vs_zndEnemy* ashley, int build)
{
    const build_t* b = &builds[build - 1];
    rng_t rng;

    rng_init(&rng, rando_config.seed, STREAM_ASHLEY | STREAM_PART_BUILD_STATS);
    ashley->hp = b->hp + rng_range(&rng, 0, 10);
    ashley->mp = b->mp + rng_range(&rng, 0, 10);
    ashley->strength = b->strength + rng_range(&rng, 0, 10);
    ashley->intelligence = b->intelligence + rng_range(&rng, 0, 10);
    ashley->agility = b->agility + rng_range(&rng, 0, 10);
}

static u16 pickItem(const u8* choices, rng_t* rng)
{
    int n = 0;

    while (n < (int)sizeof(itemChoice_t) && choices[n]) {
        ++n;
    }
    switch (choices[0]) {
    case ANY_GRIMOIRE:
        return item_randomGrimoire(rng);
    case SIGIL_NOT_CHAMOMILE:
        return rng_range(rng, CHAMOMILE_SIGIL + 1, ITEM_SIGIL_LAST);
    case BRONZE_OR_SIGIL: {
        /* As the original: ids 202-233 drawn uniformly, the eight below the
         * first allowed sigil collapsing to the Bronze Key (9 in 32). */
        int v = rng_range(rng, 0, 31);
        return v < 9 ? BRONZE_KEY : CHAMOMILE_SIGIL + 1 + (v - 9);
    }
    }
    return 0x100 + choices[rng_range(rng, 0, n - 1)];
}

/* Starting weapon (keeping the Fandango's slot and name) and items. */
void rando_buildGear(vs_inventory* inv, int build)
{
    const build_t* b = &builds[build - 1];
    vs_inventoryWeapon* w = &inv->weapons[0];
    const vs_blade* blade = gear_findBlade(b->bladeId);
    const vs_grip* grip = gear_findGrip(b->gripId);
    rng_t rng;
    int i;

    if (w->blade != 0 && blade != NULL) {
        vs_main_memcpy(&inv->blades[w->blade - 1], blade, sizeof *blade);
        inv->blades[w->blade - 1].assembledWeaponIndex = w->index;
        inv->blades[w->blade - 1].index = w->blade;
    }
    if (w->grip != 0 && grip != NULL) {
        vs_main_memcpy(&inv->grips[w->grip - 1], grip, sizeof *grip);
        inv->grips[w->grip - 1].assembledWeaponIndex = w->index;
        inv->grips[w->grip - 1].index = w->grip;
    }

    rng_init(&rng, rando_config.seed, STREAM_ASHLEY | STREAM_PART_BUILD_GEAR);
    for (i = 0; i < 5; ++i) {
        vs_inventoryMisc* m = &inv->misc[i];
        m->id = pickItem(b->items[i], &rng);
        if (!item_isConsumable(m->id)) {
            m->count = 1;
        }
    }
}
