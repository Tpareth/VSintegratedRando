/* Vagrant Story Zenith options (the_eyes' mod, as offered by ChaoticBrave's
 * randomizer), reproduced as data and code edits isolated from the Zenith
 * PPF rather than by shipping its replacement files. */
#include "rando.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* --- Perfect Analyze ------------------------------------------------------ */

/* Analyze (action 58): hit function 6 -> 2 (byte 0x15 of the record, 0xD1 ->
 * 0x41), so it no longer misses. The action table stays in RAM all session,
 * so the vanilla byte is restored for vanilla games. */
#define ANALYZE 58
#define ANALYZE_HIT_BYTE 0x15
#define ANALYZE_PERFECT 0x41

void rando_zenithAnalyze(void)
{
    if (ZENITH_EDITION) {
        return; /* the real Zenith has it */
    }
    static int taken;
    static u8 vanilla;
    u8* hit = (u8*)&vs_main_actions[ANALYZE] + ANALYZE_HIT_BYTE;

    if (!taken) {
        vanilla = *hit;
        taken = 1;
    }
    *hit = rando_enabled() && (rando_config.options & OPT_ZENITH_ANALYZE) ? ANALYZE_PERFECT : vanilla;
}

/* --- Friendly Manabreaker ------------------------------------------------- */

/* In _getDefenseGemBuff (BATTLE.PRG) a Manabreaker gem (effect 6) blocks
 * every spell; Zenith only blocks it when byte 0x33 of the spell context is
 * zero, letting friendly spells through. */
typedef struct {
    u32* at;
    u32 vanilla;
    u32 patched;
} codePatch_t;

static const codePatch_t manabreaker[] = {
    { (u32*)0x8007E6F8, 0x00000000, 0x908B0033 }, /* nop -> lbu t3, 0x33(a0) */
    { (u32*)0x8007E704, 0x24080001, 0x29680001 }, /* li t0, 1 -> slti t0, t3, 1 */
};

/* Teleport no longer takes MP: the deduction after a teleport, `subu v0, a0,
 * v1` (MP - cost), becomes MP - 0. */
static const codePatch_t teleportCost[] = {
    { (u32*)0x800736DC, 0x00831023, 0x00801023 },
};

/* Equipment drop rolls (see rando_dropRoll): `nop; slt v0, v0, v1` after
 * each roll becomes `jal stub; <item pointer -> a2>`. */
void rando_dropStubWeapon(void);
void rando_dropStubShield(void);
void rando_dropStubAccessory(void);
void rando_dropStubArmor(void);
#define JAL(f) (0x0C000000 | (((u32)(unsigned long)(f) >> 2) & 0x03FFFFFF))
#define SLT_V0_V0_V1 0x0043102A

static const codePatch_t dropRolls[] = {
    { (u32*)0x8006BB40, 0, 0 }, { (u32*)0x8006BB44, SLT_V0_V0_V1, 0x26860018 }, /* addiu a2, s4, 24: blade */
    { (u32*)0x8006BC20, 0, 0 }, { (u32*)0x8006BC24, SLT_V0_V0_V1, 0x26860018 }, /* addiu a2, s4, 24: shield */
    { (u32*)0x8006BCD4, 0, 0 }, { (u32*)0x8006BCD8, SLT_V0_V0_V1, 0x02003021 }, /* move a2, s0: accessory */
    { (u32*)0x8006BD3C, 0, 0 }, { (u32*)0x8006BD40, SLT_V0_V0_V1, 0x02203021 }, /* move a2, s1: armor */
};

/* Applies a group of patches if every word still holds its vanilla value.
 * `jals` fills in the zero patched words (they need runtime addresses). */
static void applyPatches(const codePatch_t* p, unsigned n, const u32* jals)
{
    unsigned i;

    for (i = 0; i < n; ++i) {
        if (*p[i].at != p[i].vanilla) {
            return; /* not the code we expect: leave it alone */
        }
    }
    for (i = 0; i < n; ++i) {
        *p[i].at = p[i].patched ? p[i].patched : *jals++;
    }
}

static u16 dropHistory[8][8];

/* BATTLE.PRG is freshly loaded for every game, so this only ever patches. */
void rando_zenithBattle(void)
{
    const u32 dropJals[] = { JAL(rando_dropStubWeapon), JAL(rando_dropStubShield),
        JAL(rando_dropStubAccessory), JAL(rando_dropStubArmor) };
    u32 options = rando_config.options;

    if (ZENITH_EDITION || !rando_enabled()) {
        return; /* the real Zenith has these */
    }
    if (options & OPT_ZENITH_MANABREAKER) {
        applyPatches(manabreaker, COUNT(manabreaker), NULL);
    }
    if (options & OPT_ZENITH_TELEPORT) {
        applyPatches(teleportCost, COUNT(teleportCost), NULL);
    }
    if (options & OPT_ZENITH_DROPS) {
        vs_main_bzero(dropHistory, sizeof dropHistory);
        applyPatches(dropRolls, COUNT(dropRolls), dropJals);
    }
    FlushCache();
}

/* --- Free teleport -------------------------------------------------------- */

/* MENU0.PRG's teleport menu caches Ashley's MP when it opens, greys out the
 * save points that cost more, and previews the cost on the MP gauge when one
 * is picked. Zenith caches max MP + 45 instead and skips the preview; with
 * the BATTLE.PRG patch above the teleport itself is then free. */
typedef struct {
    u8 unk0[0x1C];
    s16 currentMp;
    s16 maxMp;
} mpStats_t;

extern int menu0_teleportMp;
void menu0_setMpCost(int cost, int insufficient);

static int freeTeleport(void)
{
    return rando_enabled() && (rando_config.options & OPT_ZENITH_TELEPORT);
}

/* Replaces `teleportMp = stats->currentMp` when the menu opens. */
void rando_teleportMenuMp(const mpStats_t* stats)
{
    menu0_teleportMp = freeTeleport() ? stats->maxMp + 45 : stats->currentMp;
}

/* Replaces the cost preview when a save point is picked. */
void rando_teleportShowCost(int cost, int insufficient)
{
    if (!freeTeleport()) {
        menu0_setMpCost(cost, insufficient);
    }
}

/* --- Repeat-kill drops ---------------------------------------------------- */

/* Each equipment drop roll remembers the last 8 items of its kind it was
 * made for (armor by category). When an item has been rolled for 3 or more
 * times among those, its drop rate (out of 255) grows: by 2 per earlier roll
 * up to 5, then by a quarter of the rate per earlier roll. Rates under 4 are
 * left alone, so items that never drop still don't. The history lasts until
 * the game is next started or loaded.
 *
 * Zenith shifts the weapon history whenever any full history needs room
 * (only weapons cycle; other kinds stop recording once full). Here each
 * history cycles on its own. */
enum { DROP_WEAPON, DROP_SHIELD, DROP_ACCESSORY, DROP_ARMOR };

int rando_dropRoll(int roll, int rate, const u16* item, int kind)
{
    u16 id = *item;
    u16* history;
    int seen = 0;
    int i;

    if (rate < 4 || id == 0) {
        return roll < rate;
    }
    if (kind == DROP_ARMOR) {
        int category = ((const u8*)item)[4];
        kind += category < 5 ? category : 1;
    }
    history = dropHistory[kind];
    for (i = 0; i < 8 && history[i] != 0; ++i) {
        seen += history[i] == id;
    }
    if (i == 8) {
        for (i = 0; i < 7; ++i) {
            history[i] = history[i + 1];
        }
    }
    history[i] = id;
    if (seen >= 3) {
        rate += (seen < 6 ? 2 : rate >> 2) * seen;
    }
    return roll < rate;
}

/* --- Bosses drop Warlock Grimoires ---------------------------------------- */

/* Each boss's first spawn gets a Warlock Grimoire as its guaranteed second
 * drop; what was displaced moves down a slot. Unlike the original byte edits
 * the counts move with their items (a grimoire is always 1). Only applied
 * when the spawn still holds its vanilla drops. */
typedef struct {
    u16 roomKey;
    u16 vanillaItem2;
    u16 items[3];
    u8 counts[3];
} bossDrop_t;

static const bossDrop_t bossDrops[] = {
    /* MAP032: Cure Bulb x3, Elixir of Kings -> Grimoire Zephyr, Cure Bulb x3 */
    { 0x0D04, 0x15A, { 0x182, 0x144, 0 }, { 1, 3, 0 } },
    /* MAP286: Grimoire Foudre, Mana Bulb -> + Grimoire Teslae, Mana Tonic (chance 0, as in Zenith) */
    { 0x3302, 0x148, { 0x18F, 0x183, 0x149 }, { 1, 1, 0 } },
    /* MAP277: Grimoire Flamme, Elixir of Queens, Mana Tonic -> + Grimoire Incendie, Elixir of Queens */
    { 0x3211, 0x15D, { 0x193, 0x184, 0x15D }, { 1, 1, 255 } },
    /* MAP298: Grimoire Gaea -> + Grimoire Terre (the original left its count 0) */
    { 0x330E, 0x000, { 0x197, 0x185, 0 }, { 1, 1, 0 } },
    /* MAP312: Grimoire Avalanche, Elixir of Sages, Acolyte's Nostrum -> + Grimoire Glace, Elixir of Sages */
    { 0x3501, 0x15B, { 0x19B, 0x186, 0x15B }, { 1, 1, 255 } },
};

void rando_zenithBossDrops(vs_mpdEnemy* spawns, u32 count, u32 roomKey)
{
    unsigned i;

    if (ZENITH_EDITION || !(rando_config.options & OPT_ZENITH_BOSS_GRIMOIRES) || count == 0 || spawns[0].type != 0) {
        return;
    }
    for (i = 0; i < COUNT(bossDrops); ++i) {
        const bossDrop_t* b = &bossDrops[i];
        if (b->roomKey == roomKey && spawns[0].item2Id == b->vanillaItem2) {
            spawns[0].itemId = b->items[0];
            spawns[0].item2Id = b->items[1];
            spawns[0].item3Id = b->items[2];
            spawns[0].itemCount = b->counts[0];
            spawns[0].item2Count = b->counts[1];
            spawns[0].item3Count = b->counts[2];
            return;
        }
    }
}

/* --- Gremlins and Crimson Blades no longer cast spells -------------------- */

/* Removes the Herakles (60) and Prostasia (63) buffs from the head-limb
 * action slots of these enemies' templates. */
#define HERAKLES 60
#define PROSTASIA 63

typedef struct {
    u8 zndId;
    u8 enemy;
    u8 limbOffset; /* offset of the action within limb 2 */
    u8 spell;
} quietSlot_t;

static const quietSlot_t quietSlots[] = {
    { 32, 3, 0x14, HERAKLES }, { 32, 4, 0x14, HERAKLES },
    { 32, 12, 0x14, PROSTASIA }, { 32, 12, 0x18, HERAKLES },
    { 32, 13, 0x14, PROSTASIA }, { 32, 13, 0x18, HERAKLES },
    { 32, 21, 0x14, PROSTASIA }, { 32, 21, 0x18, HERAKLES },
    { 32, 22, 0x14, PROSTASIA }, { 32, 22, 0x18, HERAKLES },
    { 53, 10, 0x10, HERAKLES }, { 53, 11, 0x10, PROSTASIA },
    { 30, 4, 0x10, HERAKLES }, { 30, 4, 0x14, PROSTASIA },
};

#define LIMB2_OFFSET (0x238 + 2 * 0x5C)

void rando_zenithQuiet(void* data, int zndId)
{
    u32 count = *(u32*)data;
    u8* templates = (u8*)data + 4 + count * 8;
    unsigned i;

    if (ZENITH_EDITION || !(rando_config.options & OPT_ZENITH_QUIET)) {
        return;
    }
    for (i = 0; i < COUNT(quietSlots); ++i) {
        const quietSlot_t* q = &quietSlots[i];
        u8* action;
        if (q->zndId != zndId || q->enemy >= count) {
            continue;
        }
        action = templates + q->enemy * sizeof(vs_zndEnemy) + LIMB2_OFFSET + q->limbOffset;
        if (action[0] == q->spell && action[1] == 0) {
            action[0] = 0;
        }
    }
}
