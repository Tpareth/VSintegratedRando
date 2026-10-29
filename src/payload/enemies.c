/* Enemy randomization (port of ChaoticBrave's Enemies.cpp): base stats from
 * ZND enemy templates, and the chance-based third drop of MPD enemy spawns. */
#include "keys.h"
#include "rando.h"


typedef struct {
    s16 hp, mp;
    u8 strength, intelligence, agility;
} statCaps_t;

/* Balanced caps by point in the story (all minimums are 1). */
static const statCaps_t balancedCaps[] = {
    { 480, 130, 112, 127, 132 }, /* Wine Cellar to Abandoned Mines B1 */
    { 510, 500, 150, 145, 145 }, /* Town, Undercity, Abandoned Mines B2 */
    { 720, 500, 150, 145, 145 }, /* Limestone Quarry, Temple, Escapeway */
    { 790, 666, 150, 170, 155 }, /* Snowfly Forest, Forgotten Pathway, Keep, Great Cathedral */
    { 999, 999, 150, 220, 200 }, /* City Walls, Undercity West, Iron Maiden */
};

static const statCaps_t unbalancedCaps = { 999, 999, 255, 255, 255 };

/* Randomized zones and their balance group, from Enemies.cpp; zones not
 * listed (intro, ending, special areas) are left vanilla. */
static const u8 zoneGroups[][2] = {
    { 9, 0 }, { 11, 0 }, { 12, 0 }, { 13, 0 }, { 14, 0 }, { 15, 0 }, { 16, 0 }, { 17, 0 },
    { 28, 1 }, { 32, 1 }, { 36, 1 }, { 40, 1 }, { 48, 1 }, { 50, 1 },
    { 49, 2 }, { 51, 2 }, { 53, 2 },
    { 22, 3 }, { 24, 3 }, { 25, 3 }, { 30, 3 }, { 31, 3 },
    { 41, 4 }, { 52, 4 }, { 54, 4 }, { 55, 4 }, { 56, 4 },
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

int rando_zoneGroup(int zndId)
{
    unsigned i;

    for (i = 0; i < COUNT(zoneGroups); ++i) {
        if (zoneGroups[i][0] == zndId) {
            return zoneGroups[i][1];
        }
    }
    return -1;
}

static void randomizeStats(vs_zndEnemy* e, const statCaps_t* caps, rng_t* rng)
{
    e->hp = rng_range(rng, 1, caps->hp);
    e->mp = rng_range(rng, 1, caps->mp);
    e->strength = rng_range(rng, 1, caps->strength);
    e->intelligence = rng_range(rng, 1, caps->intelligence);
    e->agility = rng_range(rng, 1, caps->agility);
    e->walkSpeed = rng_range(rng, 1, 14);
    e->runSpeed = rng_range(rng, 1, 28);
}

void rando_zndEnemies(void* data, int zndId)
{
    u32 count = *(u32*)data;
    vs_zndEnemy* enemies = (vs_zndEnemy*)((u8*)data + 4 + count * 8);
    const statCaps_t* caps;
    int group = rando_zoneGroup(zndId);
    rng_t rng;
    u32 i;

    if (group < 0) {
        return;
    }
    caps = (rando_config.options & OPT_BALANCE_ENEMIES) ? &balancedCaps[group] : &unbalancedCaps;
    for (i = 0; i < count; ++i) {
        rng_init(&rng, rando_config.seed, STREAM_ENEMY_STATS | (zndId << 8) | i);
        randomizeStats(&enemies[i], caps, &rng);
    }
}

/* The third drop slot is the chance-based one (item3Count is its drop rate);
 * the first two are guaranteed drops and stay as they are, as do keys and
 * sigils. */
void rando_mpdDrops(vs_mpdEnemy* spawns, u32 count, int zndId, u32 roomKey)
{
    u16 keyItems[KEY_MAX_DRAWS] = { 0 };
    rng_t rng;
    u32 i;
    int n = 0;

    if (rando_zoneGroup(zndId) < 0) {
        return;
    }
    if (rando_config.options & OPT_LOOT_KEYS) {
        keys_roomItems(roomKey, KEY_PASS_DROPS, keyItems);
    }
    for (i = 0; i < count; ++i) {
        vs_mpdEnemy* s = &spawns[i];
        if (s->type != 0 || item_isProgression(s->item3Id)) {
            continue;
        }
        rng_init(&rng, rando_config.seed, STREAM_ENEMY_DROPS | (roomKey << 8) | i);
        s->item3Id = (n < KEY_MAX_DRAWS && keyItems[n]) ? keyItems[n] : item_randomMisc(&rng);
        s->item3Count = rng_range(&rng, 1, 100);
        ++n;
    }
}
