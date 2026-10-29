/* Ability randomization (port of ChaoticBrave's Ashley::breakArts, chainCost,
 * abilityCost, ultiSpells and abiFrame). The tables live in SLUS_010.40's
 * data, which stays in RAM for the whole session, so every time the battle
 * program starts they are first restored from a snapshot of the vanilla
 * values (a vanilla save loaded after a randomized one must not keep them),
 * then randomized if this game is. BATTLE.PRG is reloaded from disc for every
 * game, so its code patch needs no restoring. */
#include "rando.h"

#define BREAK_ART_FIRST 184 /* vs_main_actions index of the first Break Art */
#define BREAK_ART_COUNT 40
#define CHAIN_COUNT 22
#define ULTIMATE_COUNT 7
#define ULTIMATE_FIRST 134 /* Tornado .. Apocalypse, enemy-only ultimates */
#define LEVEL4_FIRST 109 /* Explosion Lv.4, then every 4th action */

/* Chain and defense abilities whose risk cost is randomized (Heavy Shot ..
 * Phantom Shield). */
static const u8 riskAbilities[] = {
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 37,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* `li s0, 8` in BATTLE.PRG: the frame windows for chain and defense
 * abilities (10 frames in the other branch is left alone). */
#ifdef RANDO_HOST
extern u32* const frameWindowCode[2]; /* provided by the test harness */
#else
static u32* const frameWindowCode[] = { (u32*)0x80078B30, (u32*)0x80078B48 };
#endif
#define LI_S0 0x24100000

enum {
    STREAM_PART_BREAK_ARTS,
    STREAM_PART_CHAIN,
    STREAM_PART_RISK,
    STREAM_PART_FRAMES,
};

static struct {
    int taken;
    u8 breakArtCosts[BREAK_ART_COUNT];
    u16 breakArtPoints[BREAK_ART_COUNT];
    u16 chainPoints[CHAIN_COUNT];
    u8 riskCosts[COUNT(riskAbilities)];
    vs_action_t level4[ULTIMATE_COUNT];
} vanilla;

/* The action records (costs, the Lv.4 spells) are only touched in the
 * vanilla edition: Zenith rearranges vs_main_actions (see rando.h). */
static void snapshot(void)
{
    unsigned i;

    for (i = 0; i < BREAK_ART_COUNT; ++i) {
        vanilla.breakArtPoints[i] = vs_main_artsPointsRequirements[i];
    }
    for (i = 0; i < CHAIN_COUNT; ++i) {
        vanilla.chainPoints[i] = vs_main_battleAbilitiesPointsRequirements[i];
    }
    vanilla.taken = 1;
    if (ZENITH_EDITION) {
        return;
    }
    for (i = 0; i < BREAK_ART_COUNT; ++i) {
        vanilla.breakArtCosts[i] = vs_main_actions[BREAK_ART_FIRST + i].cost;
    }
    for (i = 0; i < COUNT(riskAbilities); ++i) {
        vanilla.riskCosts[i] = vs_main_actions[riskAbilities[i]].cost;
    }
    for (i = 0; i < ULTIMATE_COUNT; ++i) {
        vanilla.level4[i] = vs_main_actions[LEVEL4_FIRST + i * 4];
    }
}

static void restore(void)
{
    unsigned i;

    for (i = 0; i < BREAK_ART_COUNT; ++i) {
        vs_main_artsPointsRequirements[i] = vanilla.breakArtPoints[i];
    }
    for (i = 0; i < CHAIN_COUNT; ++i) {
        vs_main_battleAbilitiesPointsRequirements[i] = vanilla.chainPoints[i];
    }
    if (ZENITH_EDITION) {
        return;
    }
    for (i = 0; i < BREAK_ART_COUNT; ++i) {
        vs_main_actions[BREAK_ART_FIRST + i].cost = vanilla.breakArtCosts[i];
    }
    for (i = 0; i < COUNT(riskAbilities); ++i) {
        vs_main_actions[riskAbilities[i]].cost = vanilla.riskCosts[i];
    }
    for (i = 0; i < ULTIMATE_COUNT; ++i) {
        vs_main_actions[LEVEL4_FIRST + i * 4] = vanilla.level4[i];
    }
}

static void rngFor(rng_t* rng, int part)
{
    rng_init(rng, rando_config.seed, STREAM_ABILITIES | part);
}

/* Costs to use and points to learn each Break Art; balanced keeps them in
 * the vanilla ranges (25-75 and 10-485). */
static void breakArts(void)
{
    int balanced = rando_config.options & OPT_BALANCE_BREAK_ARTS;
    rng_t rng;
    int i;

    rngFor(&rng, STREAM_PART_BREAK_ARTS);
    for (i = 0; i < BREAK_ART_COUNT; ++i) {
        int cost = balanced ? rng_range(&rng, 25, 75) : rng_range(&rng, 0, 255);
        if (!ZENITH_EDITION) {
            vs_main_actions[BREAK_ART_FIRST + i].cost = cost; /* rolled anyway, so the points match */
        }
        vs_main_artsPointsRequirements[i] = balanced ? rng_range(&rng, 10, 485) : rng_range(&rng, 1, 999);
    }
}

/* Points to learn each battle ability: increasing, each at most its vanilla
 * value. */
static void chainLearning(void)
{
    rng_t rng;
    int low = 0;
    int i;

    rngFor(&rng, STREAM_PART_CHAIN);
    for (i = 0; i < CHAIN_COUNT; ++i) {
        int points = rng_range(&rng, low, vanilla.chainPoints[i]);
        vs_main_battleAbilitiesPointsRequirements[i] = points;
        low = points + 1;
    }
}

static void riskCosts(void)
{
    rng_t rng;
    unsigned i;

    rngFor(&rng, STREAM_PART_RISK);
    for (i = 0; i < COUNT(riskAbilities); ++i) {
        vs_main_actions[riskAbilities[i]].cost = rng_range(&rng, 1, 6);
    }
}

/* Each Lv.4 elemental spell becomes the matching enemy ultimate, keeping the
 * Lv.4 spell's cost type, range and area of effect. */
static void ultimateSpells(void)
{
    int i;

    for (i = 0; i < ULTIMATE_COUNT; ++i) {
        u8* spell = (u8*)&vs_main_actions[LEVEL4_FIRST + i * 4];
        const u8* level4 = (const u8*)&vanilla.level4[i];
        vs_main_memcpy(spell, &vs_main_actions[ULTIMATE_FIRST + i], sizeof(vs_action_t));
        spell[2] = level4[2];
        vs_main_memcpy(spell + 4, level4 + 4, 8);
    }
}

static void frameWindows(void)
{
    rng_t rng;
    unsigned i;
    int frames;

    rngFor(&rng, STREAM_PART_FRAMES);
    frames = rng_range(&rng, 8, 30);
    for (i = 0; i < COUNT(frameWindowCode); ++i) {
        *frameWindowCode[i] = LI_S0 | frames;
    }
    FlushCache();
}

/* Called each time the battle program starts (New Game and Continue). */
void rando_abilities(void)
{
    u32 options;

    if (!vanilla.taken) {
        snapshot();
    }
    restore();
    if (!rando_enabled()) {
        return;
    }
    options = rando_config.options;
    if (options & OPT_BREAK_ARTS) {
        breakArts();
    }
    if (options & OPT_CHAIN_LEARNING) {
        chainLearning();
    }
    if ((options & OPT_RISK_COSTS) && !ZENITH_EDITION) {
        riskCosts();
    }
    if ((options & OPT_ULTIMATE_SPELLS) && !ZENITH_EDITION) {
        ultimateSpells();
    }
    if (options & OPT_FRAME_WINDOWS) {
        frameWindows();
    }
}
