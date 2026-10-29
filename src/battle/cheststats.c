/* Chest equipment stat rolls (the class, affinity and BEP parts of
 * ChaoticBrave's Chests::mapIterate, plus the random DP/PP and gem stats of
 * its base stats option). Runs after the chest contents are chosen, on
 * whatever equipment the chest then holds.
 *
 * The PC randomizer's base option also replaces each item with a random one
 * and fills in that item's real stats from tables; here the "Chest
 * equipment" option already picks real items, so only the random parts are
 * kept. "Balance item stats" narrows the ranges by map, in four tiers
 * (build/gen/chesttiers.c). */
#include "rando.h"
#include "roll.h"

extern const u32 chestTiers[];
extern const int chestTierCount;

/* Ranges per tier (early, middle, late, everywhere else). */
static const stat_t classAffinity = { ANY, { SPLIT(20, 5), SPLIT(20, 6), SPLIT(20, 11), SPLIT(30, 15) } };
static const stat_t bep = { ANY, { R(0, 7), R(0, 7), R(0, 20), R(0, 25) } };
static const stat_t gemStats = { ANY, { { 0, 5, 3 }, { 0, 6, 3 }, { 0, 6, 3 }, { 0, 15, 3 } } };

#define CLASS_COUNT 6
#define AFFINITY_COUNT 7

/* Inventory-format record offsets (vs_blade, vs_grip, vs_gem, vs_armor). */
enum {
    BLADE_MAX_DP = 0x04,
    BLADE_CLASSES = 0x18,
    BLADE_AFFINITIES = 0x20,
    GRIP_BEP = 0x09,
    GEM_STR = 0x05,
    GEM_CLASSES = 0x08,
    GEM_AFFINITIES = 0x10,
    ARMOR_MAX_DP = 0x04,
    ARMOR_BEP = 0x11,
    ARMOR_CLASSES = 0x14,
    ARMOR_AFFINITIES = 0x1C,
};

/* Blade and Fandango cost stat options: a new cost type (1-4, which stat
 * using the blade costs), keeping the damage type (Chests.cpp and
 * Ashley.cpp: 4 * (1-4) + damage type). */
void rando_rollCostType(rng_t* rng, vs_blade* blade)
{
    blade->damageCostType = rng_range(rng, 1, 4) << 2 | (blade->damageCostType & 3);
}

static int tierOf(u32 roomKey)
{
    int i;

    for (i = 0; i < chestTierCount; ++i) {
        if (chestTiers[i] >> 2 == roomKey) {
            return chestTiers[i] & 3;
        }
    }
    return 3;
}

/* maxDp, maxPp, currentDp, currentPp as in vs_blade and vs_armor: DP
 * 4100-32700 in steps of 100 with the current value up to it, PP 100-250. */
static void durability(roller_t* r, u8* dp, int pp)
{
    int maxDp = rng_range(&r->rng, 41, 327);

    put16(dp, maxDp * 100);
    put16(dp + 4, rng_range(&r->rng, 41, maxDp) * 100);
    if (pp) {
        int maxPp = rng_range(&r->rng, 100, 250);
        put16(dp + 2, maxPp);
        put16(dp + 6, rng_range(&r->rng, 1, maxPp));
    }
}

static void classesAffinities(roller_t* r, u8* classes, u8* affinities, u32 opts)
{
    if (opts & OPT2_CHEST_GEAR_CLASS) {
        rollBytes(r, classes, CLASS_COUNT, &classAffinity);
    }
    if (opts & OPT2_CHEST_GEAR_AFFINITY) {
        rollBytes(r, affinities, AFFINITY_COUNT, &classAffinity);
    }
}

static void blade(roller_t* r, vs_blade* blade, u32 opts)
{
    u8* b = (u8*)blade;

    if (blade->id == 0) {
        return;
    }
    if (opts & OPT2_BLADE_COST) {
        rando_rollCostType(&r->rng, blade);
    }
    if (opts & OPT2_CHEST_GEAR_BASE) {
        durability(r, b + BLADE_MAX_DP, 1);
    }
    classesAffinities(r, b + BLADE_CLASSES, b + BLADE_AFFINITIES, opts);
}

static void grip(roller_t* r, vs_grip* grip, u32 opts)
{
    if (grip->id != 0 && (opts & OPT2_CHEST_GEAR_BEP)) {
        rollBytes(r, (u8*)grip + GRIP_BEP, 3, &bep);
    }
}

static void gem(roller_t* r, vs_gem* gem, u32 opts)
{
    u8* g = (u8*)gem;

    if (gem->id == 0) {
        return;
    }
    if (opts & OPT2_CHEST_GEAR_BASE) {
        rollBytes(r, g + GEM_STR, 3, &gemStats);
    }
    classesAffinities(r, g + GEM_CLASSES, g + GEM_AFFINITIES, opts);
}

/* Shields, armor and accessories (accessories have no DP or BEP). */
static void armor(roller_t* r, vs_armor* armor, int pp, u32 opts)
{
    u8* a = (u8*)armor;
    int accessory = armor->category == ARMOR_ACCESSORY;

    if (armor->id == 0) {
        return;
    }
    if ((opts & OPT2_CHEST_GEAR_BASE) && !accessory) {
        durability(r, a + ARMOR_MAX_DP, pp);
    }
    if ((opts & OPT2_CHEST_GEAR_BEP) && !accessory) {
        rollBytes(r, a + ARMOR_BEP, 3, &bep);
    }
    classesAffinities(r, a + ARMOR_CLASSES, a + ARMOR_AFFINITIES, opts);
}

enum { PART_WEAPON, PART_BLADE, PART_GRIP, PART_SHIELD, PART_ARMOR0, PART_ARMOR1, PART_ACCESSORY, PART_GEM };

#define CHEST_OPTIONS (OPT2_CHEST_GEAR_BASE | OPT2_CHEST_GEAR_CLASS | OPT2_CHEST_GEAR_AFFINITY | OPT2_CHEST_GEAR_BEP \
    | OPT2_BLADE_COST)

void rando_chestStats(vs_loot* loot, u32 roomKey)
{
    u32 opts = rando_config.options2;
    roller_t r;
    int part;
    int i;

    if (!(opts & CHEST_OPTIONS)) {
        return;
    }
    r.group = (opts & OPT2_BALANCE_ITEM_STATS) ? tierOf(roomKey) : -1;
    /* A stream per piece, so one piece or option does not shift another's rolls. */
    for (part = PART_WEAPON; part <= PART_GEM; ++part) {
        rng_init(&r.rng, rando_config.seed, STREAM_ITEM_STATS | roomKey << 8 | part);
        switch (part) {
        case PART_WEAPON:
            if (loot->weapon.difficultyFlags) {
                blade(&r, &loot->weapon.blade, opts);
                grip(&r, &loot->weapon.grip, opts);
                for (i = 0; i < 3; ++i) {
                    gem(&r, &loot->weapon.gems[i], opts);
                }
            }
            break;
        case PART_BLADE:
            if (loot->blade.difficultyFlags) {
                blade(&r, &loot->blade.blade, opts);
            }
            break;
        case PART_GRIP:
            if (loot->grip.difficultyFlags) {
                grip(&r, &loot->grip.grip, opts);
            }
            break;
        case PART_SHIELD:
            if (loot->shield.difficultyFlags) {
                armor(&r, &loot->shield.shield, 1, opts);
                for (i = 0; i < 3; ++i) {
                    gem(&r, &loot->shield.gems[i], opts);
                }
            }
            break;
        case PART_ARMOR0:
        case PART_ARMOR1:
            if (loot->armor[part - PART_ARMOR0].difficultyFlags) {
                armor(&r, &loot->armor[part - PART_ARMOR0].armor, 0, opts);
            }
            break;
        case PART_ACCESSORY:
            if (loot->accessory.difficultyFlags) {
                armor(&r, &loot->accessory.armor, 0, opts);
            }
            break;
        case PART_GEM:
            if (loot->gem.difficultyFlags) {
                gem(&r, &loot->gem.gem, opts);
            }
            break;
        }
    }
}
