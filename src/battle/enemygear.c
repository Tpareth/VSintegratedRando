/* Enemy equipment and appendage randomization (port of the equipment part of
 * ChaoticBrave's Enemies::mapIterate). Works on the ZND enemy templates
 * (vs_battle_charInitData) after the base stats are rolled.
 *
 * Ranges are the PC randomizer's. Balanced ranges depend on the zone's
 * story group (rando_zoneGroup); a "split" roll is a coin flip between a
 * small positive and a small negative range. Departures from the PC code,
 * where it does not do what it evidently means to:
 * - every class (6) and affinity (7) byte is rolled; the PC writes a stale
 *   value into the first class byte and drops the last roll;
 * - new ids come from the vanilla equipment pools of the same category, so
 *   the sub id and weapon model id match the item (the PC writes the blade
 *   id as its model id, and fixed offsets for shields and accessories);
 * - armor ids follow the piece's own body slot, not the limb index;
 * - only existing limbs (HP > 0) are changed, and never to 0 HP;
 * - shield classes in the last two groups use the split roll the earlier
 *   groups use (the PC leaves a stale value there). */
#include "equipment.h"
#include "rando.h"
#include "roll.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* --- template layout ------------------------------------------------------- */

/* Equipment records in a template are vs_battle_uiEquipment (0x30 bytes). */
enum {
    EQ_ID = 0x00, /* u16 */
    EQ_SUB_ID = 0x02,
    EQ_WEP_ID = 0x03,
    EQ_CATEGORY = 0x04,
    EQ_STR = 0x05, /* STR, INT, AGL */
    EQ_CUR_DP = 0x08, /* u16 */
    EQ_MAX_DP = 0x0A,
    EQ_CUR_PP = 0x0C,
    EQ_MAX_PP = 0x0E,
    EQ_DAMAGE_TYPE = 0x10,
    EQ_COST_TYPE = 0x11,
    EQ_COST = 0x12,
    EQ_GEM_SLOTS = 0x15,
    EQ_RANGE = 0x18, /* 3 bytes rolled */
    EQ_BEP = 0x1D, /* types[1..3]: blunt, edged, piercing */
    EQ_CLASSES = 0x20,
    EQ_AFFINITIES = 0x28,
    EQ_SIZE = 0x30,
};

enum {
    T_HP = 0x1C,
    T_BLADE = 0x34,
    T_GRIP = 0x64,
    T_WEAPON_GEMS = 0x94,
    T_WEAPON_MATERIAL = 0x124,
    T_WEAPON_DROP = 0x125,
    T_SHIELD = 0x140,
    T_SHIELD_GEMS = 0x170,
    T_SHIELD_MATERIAL = 0x200,
    T_SHIELD_DROP = 0x201,
    T_ACCESSORY = 0x204,
    T_ACCESSORY_DROP = 0x234,
    T_LIMBS = 0x238,
    LIMB_SIZE = 0x5C,
    LIMB_HP = 0x00, /* u16 */
    LIMB_AGILITY_DEFENSE = 0x02,
    LIMB_CHAIN_EVASION = 0x03,
    LIMB_BEP = 0x05,
    LIMB_AFFINITIES = 0x08,
    LIMB_ARMOR = 0x20,
    LIMB_ARMOR_MATERIAL = 0x50,
    LIMB_ARMOR_DROP = 0x51,
};

#define CLASS_COUNT 6
#define AFFINITY_COUNT 7

#define ALL5(r) { r, r, r, r, r }

static const stat_t bladeStr = { R(1, 255), { R(1, 14), R(1, 31), R(1, 33), R(1, 43), R(1, 43) } };
static const stat_t bladeInt = { R(1, 255), { R(1, 4), R(1, 26), R(1, 26), R(1, 26), R(1, 28) } };
static const stat_t bladeAgl = { R(1, 255), { SPLIT(0, 7), SPLIT(0, 7), SPLIT(0, 7), SPLIT(0, 8), SPLIT(0, 8) } };
static const stat_t bladeClass = { ANY, { SPLIT(14, 20), SPLIT(20, 20), SPLIT(20, 20), SPLIT(20, 20), SPLIT(20, 20) } };
static const stat_t bladeAffinity = { ANY, { SPLIT(10, 3), SPLIT(20, 5), SPLIT(20, 5), SPLIT(30, 5), SPLIT(30, 10) } };
static const stat_t weaponMaterial = { R(3, 7), { R(3, 4), R(3, 6), R(3, 6), R(3, 7), R(3, 7) } };

static const stat_t gripStrInt = { ANY, ALL5(R(0, 12)) };
static const stat_t gripAgl = { ANY, { R(-3, -1), R(-4, -1), R(-4, -1), R(-4, -1), R(-4, -1) } };
static const stat_t gripBep = { ANY, { R(0, 13), R(0, 21), R(0, 21), R(0, 21), R(0, 21) } };

static const stat_t gemStats = { ANY, { R(0, 5), R(0, 6), R(0, 6), { 0, 12, 3 }, { 0, 15, 3 } } };
static const stat_t gemClass = { ANY, { SPLIT(15, 3), SPLIT(15, 3), SPLIT(15, 3), SPLIT(15, 5), SPLIT(30, 6) } };
static const stat_t gemAffinity = { ANY, { R(0, 0), SPLIT(30, 15), SPLIT(30, 15), SPLIT(30, 15), SPLIT(30, 15) } };

static const stat_t shieldStrInt = { ANY, { R(4, 9), R(4, 20), R(4, 20), R(4, 22), R(4, 24) } };
static const stat_t shieldAgl = { ANY, { R(-3, -1), R(-4, -1), R(-4, -1), R(-4, -1), R(-4, -1) } };
static const stat_t shieldBep = { ANY, { R(0, 10), R(0, 20), R(0, 20), R(0, 25), R(0, 25) } };
static const stat_t shieldClass = { ANY, { R(-5, -1), SPLIT(10, 5), SPLIT(10, 5), SPLIT(14, 5), SPLIT(15, 5) } };
static const stat_t shieldAffinity = { ANY, { SPLIT(8, 6), SPLIT(14, 6), SPLIT(14, 6), SPLIT(15, 6), SPLIT(19, 6) } };
static const stat_t armorMaterial = { R(1, 7), { R(1, 4), R(1, 7), R(1, 7), R(1, 7), R(1, 7) } };

static const stat_t accessoryStats = { ANY, { R(1, 3), R(1, 5), R(1, 5), R(1, 5), R(1, 25) } };
static const stat_t accessoryBep = { ANY, { R(0, 0), R(0, 15), R(0, 15), R(0, 20), SPLIT(20, 20) } };
static const stat_t accessoryClass = { ANY, { SPLIT(10, 10), SPLIT(15, 10), SPLIT(30, 12), SPLIT(30, 12), SPLIT(30, 12) } };
static const stat_t accessoryAffinity = { ANY, { SPLIT(10, 5), SPLIT(30, 10), SPLIT(30, 10), SPLIT(30, 10), SPLIT(30, 25) } };

static const stat_t limbAgilityDefense = { ANY, ALL5(R(0, 251)) };
static const stat_t limbChainEvasion = { ANY, { ANY, R(0, 251), ANY, ANY, ANY } };
static const stat_t limbBep = { ANY, { SPLIT(35, 30), SPLIT(38, 30), SPLIT(55, 30), SPLIT(55, 30), SPLIT(60, 30) } };
static const stat_t limbAffinity = { ANY, { SPLIT(75, 33), SPLIT(100, 33), SPLIT(100, 33), SPLIT(100, 33), SPLIT(100, 33) } };

static const stat_t bodyArmorStrInt = { ANY, { R(1, 13), R(1, 17), R(1, 17), R(1, 17), R(1, 17) } };
static const stat_t armorStrInt = { ANY, { R(1, 13), R(1, 25), R(1, 25), R(1, 25), R(1, 25) } };
static const stat_t armorAgl = { ANY, { SPLIT(0, 3), SPLIT(0, 4), SPLIT(0, 4), SPLIT(0, 4), SPLIT(0, 4) } };
static const stat_t armorBep = { ANY, { R(0, 5), R(0, 12), R(0, 12), R(0, 12), R(0, 12) } };
static const stat_t armorClass = { ANY, { SPLIT(1, 5), SPLIT(20, 5), SPLIT(20, 5), SPLIT(24, 5), SPLIT(27, 5) } };
static const stat_t armorAffinity = { ANY, { SPLIT(10, 5), SPLIT(23, 17), SPLIT(23, 17), SPLIT(23, 17), SPLIT(24, 17) } };

/* Blade id ranges per category (1 dagger .. 9 crossbow), as rolled by the PC
 * randomizer; ids are then taken from the vanilla pool within the range. */
static const u8 bladeIds[9][2] = {
    { 1, 12 }, { 13, 24 }, { 25, 34 }, { 35, 50 }, { 51, 56 }, { 57, 62 }, { 63, 69 }, { 70, 82 }, { 83, 90 },
};

/* Maximum DP 4100-32700 in steps of 100, current DP 4100 up to it; PP
 * 100-250, current PP 1 up to it. */
static void rollDurability(roller_t* r, u8* e, int pp)
{
    int maxDp = rng_range(&r->rng, 41, 327);

    put16(e + EQ_MAX_DP, maxDp * 100);
    put16(e + EQ_CUR_DP, rng_range(&r->rng, 41, maxDp) * 100);
    if (pp) {
        int maxPp = rng_range(&r->rng, 100, 250);
        put16(e + EQ_MAX_PP, maxPp);
        put16(e + EQ_CUR_PP, rng_range(&r->rng, 1, maxPp));
    }
}

/* --- equipment ids from the vanilla pools ----------------------------------- */

static int bladeInRange(const vs_blade* b, int category)
{
    return b->id >= bladeIds[category - 1][0] && b->id <= bladeIds[category - 1][1];
}

static void newBladeId(roller_t* r, u8* e)
{
    int category = e[EQ_CATEGORY];
    int matches = 0;
    int i, n;

    if (category < 1 || category > 9) {
        return;
    }
    for (i = 0; i < pool_bladeCount; ++i) {
        matches += bladeInRange((const vs_blade*)pool_blades[i], category);
    }
    if (matches == 0) {
        return;
    }
    n = rng_range(&r->rng, 0, matches - 1);
    for (i = 0; i < pool_bladeCount; ++i) {
        const vs_blade* b = (const vs_blade*)pool_blades[i];
        if (bladeInRange(b, category) && n-- == 0) {
            put16(e + EQ_ID, b->id);
            e[EQ_SUB_ID] = b->subId;
            e[EQ_WEP_ID] = b->wepId;
            return;
        }
    }
}

static void newGripId(roller_t* r, u8* e)
{
    const vs_grip* g = gear_pickGrip(&r->rng, e[EQ_CATEGORY]);

    if (g != NULL) {
        put16(e + EQ_ID, g->id);
        e[EQ_SUB_ID] = g->subId;
    }
}

/* Shields, armor and accessories share the inventory armor record. */
static void newArmorId(roller_t* r, u8* e, const vs_armor* a)
{
    if (a != NULL) {
        put16(e + EQ_ID, a->id);
        e[EQ_SUB_ID] = a->subId;
        e[EQ_WEP_ID] = a->wepId;
    }
}

/* --- per equipment kind ----------------------------------------------------- */

static void rollClassAffinity(roller_t* r, u8* e, u32 opts, const stat_t* cls, const stat_t* aff)
{
    if (opts & OPT2_ENEMY_GEAR_CLASS) {
        rollBytes(r, e + EQ_CLASSES, CLASS_COUNT, cls);
    }
    if (opts & OPT2_ENEMY_GEAR_AFFINITY) {
        rollBytes(r, e + EQ_AFFINITIES, AFFINITY_COUNT, aff);
    }
}

/* Gem slots up to the holder's count; with base stats on, empty slots get a
 * random gem, as in the PC randomizer. */
static void gems(roller_t* r, u8* gem, int slots, u32 opts)
{
    int i;

    for (i = 0; i < slots && i < 3; ++i, gem += EQ_SIZE) {
        if (opts & OPT2_ENEMY_GEAR_BASE) {
            const vs_gem* g = gear_pickGem(&r->rng);
            if (g != NULL) {
                put16(gem + EQ_ID, g->id);
                gem[EQ_SUB_ID] = g->subId;
            }
            rollBytes(r, gem + EQ_STR, 3, &gemStats);
        }
        if (get16(gem + EQ_ID) != 0) {
            rollClassAffinity(r, gem, opts, &gemClass, &gemAffinity);
        }
    }
}

static void weapon(roller_t* r, u8* t, u32 opts)
{
    u8* blade = t + T_BLADE;
    u8* grip = t + T_GRIP;

    if (blade[EQ_CATEGORY] == 0) {
        return;
    }
    if (opts & OPT2_ENEMY_GEAR_BASE) {
        newBladeId(r, blade);
        blade[EQ_STR] = roll(r, &bladeStr);
        blade[EQ_STR + 1] = roll(r, &bladeInt);
        blade[EQ_STR + 2] = roll(r, &bladeAgl);
        rollDurability(r, blade, 1);
        blade[EQ_DAMAGE_TYPE] = rng_range(&r->rng, 1, 3);
        blade[EQ_COST_TYPE] = rng_range(&r->rng, 1, 4);
        blade[EQ_COST] = rng_range(&r->rng, 1, 255);
        if (r->group < 0) {
            int i;
            for (i = 0; i < 3; ++i) {
                blade[EQ_RANGE + i] = rng_range(&r->rng, 1, 255);
            }
        }
        newGripId(r, grip);
        grip[EQ_STR] = roll(r, &gripStrInt);
        grip[EQ_STR + 1] = roll(r, &gripStrInt);
        grip[EQ_STR + 2] = roll(r, &gripAgl);
        t[T_WEAPON_MATERIAL] = roll(r, &weaponMaterial);
        t[T_WEAPON_DROP] = rng_range(&r->rng, 0, 255);
    }
    rollClassAffinity(r, blade, opts, &bladeClass, &bladeAffinity);
    if (opts & OPT2_ENEMY_GEAR_BEP) {
        rollBytes(r, grip + EQ_BEP, 3, &gripBep);
    }
    gems(r, t + T_WEAPON_GEMS, grip[EQ_GEM_SLOTS], opts);
}

static void shield(roller_t* r, u8* t, u32 opts)
{
    u8* s = t + T_SHIELD;

    if (get16(s + EQ_ID) == 0) {
        return;
    }
    if (opts & OPT2_ENEMY_GEAR_BASE) {
        newArmorId(r, s, gear_pickShield(&r->rng, MATERIAL_ANY));
        s[EQ_STR] = roll(r, &shieldStrInt);
        s[EQ_STR + 1] = roll(r, &shieldStrInt);
        s[EQ_STR + 2] = roll(r, &shieldAgl);
        rollDurability(r, s, 1);
        t[T_SHIELD_MATERIAL] = roll(r, &armorMaterial);
        t[T_SHIELD_DROP] = rng_range(&r->rng, 0, 255);
    }
    if (opts & OPT2_ENEMY_GEAR_BEP) {
        rollBytes(r, s + EQ_BEP, 3, &shieldBep);
    }
    rollClassAffinity(r, s, opts, &shieldClass, &shieldAffinity);
    gems(r, t + T_SHIELD_GEMS, s[EQ_GEM_SLOTS], opts);
}

static void accessory(roller_t* r, u8* t, u32 opts)
{
    u8* a = t + T_ACCESSORY;

    if (get16(a + EQ_ID) == 0) {
        return;
    }
    if (opts & OPT2_ENEMY_GEAR_BASE) {
        newArmorId(r, a, gear_pickArmor(&r->rng, ARMOR_ACCESSORY, MATERIAL_ANY));
        rollBytes(r, a + EQ_STR, 3, &accessoryStats);
        t[T_ACCESSORY_DROP] = rng_range(&r->rng, 0, 255);
    }
    if (opts & OPT2_ENEMY_GEAR_BEP) {
        rollBytes(r, a + EQ_BEP, 3, &accessoryBep);
    }
    rollClassAffinity(r, a, opts, &accessoryClass, &accessoryAffinity);
}

static void armor(roller_t* r, u8* limb, u32 opts)
{
    u8* a = limb + LIMB_ARMOR;
    int category = a[EQ_CATEGORY];

    if (category == 0) {
        return;
    }
    if (opts & OPT2_ENEMY_GEAR_BASE) {
        const stat_t* strInt = category == ARMOR_BODY ? &bodyArmorStrInt : &armorStrInt;
        if (category >= ARMOR_HEAD && category <= ARMOR_ARMS) {
            newArmorId(r, a, gear_pickArmor(&r->rng, category, MATERIAL_ANY));
        }
        a[EQ_STR] = roll(r, strInt);
        a[EQ_STR + 1] = roll(r, strInt);
        a[EQ_STR + 2] = roll(r, &armorAgl);
        rollDurability(r, a, 0);
        limb[LIMB_ARMOR_MATERIAL] = roll(r, &armorMaterial);
        limb[LIMB_ARMOR_DROP] = rng_range(&r->rng, 0, 255);
    }
    if (opts & OPT2_ENEMY_GEAR_BEP) {
        rollBytes(r, a + EQ_BEP, 3, &armorBep);
    }
    rollClassAffinity(r, a, opts, &armorClass, &armorAffinity);
}

/* Body parts: HP as a share of the enemy's (1/2 for the first two, 1/3 for
 * the third and sixth, all of it for the fourth and fifth), defenses, and
 * with the BEP/affinity options their BEP and affinities. */
static const u8 limbHpDivisor[6] = { 2, 2, 3, 1, 1, 3 };

static void appendage(roller_t* r, u8* limb, int index, int enemyHp, u32 opts)
{
    int max = enemyHp / limbHpDivisor[index];

    if (get16(limb + LIMB_HP) == 0) {
        return; /* no such part */
    }
    put16(limb + LIMB_HP, rng_range(&r->rng, 1, max > 1 ? max : 1));
    limb[LIMB_AGILITY_DEFENSE] = roll(r, &limbAgilityDefense);
    limb[LIMB_CHAIN_EVASION] = roll(r, &limbChainEvasion);
    if (opts & OPT2_ENEMY_GEAR_BEP) {
        rollBytes(r, limb + LIMB_BEP, 3, &limbBep);
    }
    if (opts & OPT2_ENEMY_GEAR_AFFINITY) {
        rollBytes(r, limb + LIMB_AFFINITIES, AFFINITY_COUNT, &limbAffinity);
    }
}

/* Ice Wolf option: the City Walls (zone 28) Hellhound, template 0, gets the
 * Snow Dragon's ice breath (action 159 for its fire breath, 147) and its
 * Fire and Water affinities swapped. Only applied to the vanilla values. */
#define ICE_WOLF_ZONE 28
#define FIRE_BREATH 147
#define ICE_BREATH 159

static void iceWolf(u8* t)
{
    u8* head = t + T_LIMBS;
    u8* breath = head + 0x14; /* actions[1].id */
    s8* aff = (s8*)head + LIMB_AFFINITIES;

    if (get16(breath) == FIRE_BREATH && aff[2] == 45 && aff[4] == -25) {
        put16(breath, ICE_BREATH);
        aff[2] = -25;
        aff[4] = 45;
    }
}

enum { PART_WEAPON, PART_SHIELD, PART_ACCESSORY, PART_LIMBS = 3 /* .. 8 */, PART_APPENDAGES = 9 /* .. 14 */ };

#define GEAR_OPTIONS (OPT2_ENEMY_GEAR_BASE | OPT2_ENEMY_GEAR_CLASS | OPT2_ENEMY_GEAR_AFFINITY | OPT2_ENEMY_GEAR_BEP)

void rando_enemyGear(void* data, int zndId)
{
    u32 count = *(u32*)data;
    u8* templates = (u8*)data + 4 + count * 8;
    u32 opts = rando_config.options2;
    int group = rando_zoneGroup(zndId);
    roller_t r;
    u32 i;
    int part;

    if (zndId == ICE_WOLF_ZONE && count > 0 && (opts & OPT2_ICE_WOLF)) {
        iceWolf(templates);
    }
    if (group < 0 || !(opts & (GEAR_OPTIONS | OPT2_ENEMY_APPENDAGES))) {
        return;
    }
    if (!(rando_config.options & OPT_BALANCE_ENEMIES)) {
        group = -1;
    }
    r.group = group;
    for (i = 0; i < count; ++i) {
        u8* t = templates + i * sizeof(vs_zndEnemy);
        u32 stream = STREAM_ENEMY_GEAR | zndId << 8 | i;
        /* A stream per part, so one option or piece does not shift another's rolls. */
        for (part = 0; part < PART_APPENDAGES + 6; ++part) {
            rng_init(&r.rng, rando_config.seed, stream ^ part << 16);
            if (part == PART_WEAPON) {
                weapon(&r, t, opts);
            } else if (part == PART_SHIELD) {
                shield(&r, t, opts);
            } else if (part == PART_ACCESSORY) {
                accessory(&r, t, opts);
            } else if (part < PART_APPENDAGES) {
                armor(&r, t + T_LIMBS + (part - PART_LIMBS) * LIMB_SIZE, opts);
            } else if (opts & OPT2_ENEMY_APPENDAGES) {
                int limb = part - PART_APPENDAGES;
                appendage(&r, t + T_LIMBS + limb * LIMB_SIZE, limb, (s16)get16(t + T_HP), opts);
            }
        }
    }
}

/* --- Ashley ------------------------------------------------------------------ */

/* Ashley's class, affinity and BEP options (Ashley::statIterate): the PC
 * randomizer's balanced ranges for him are the first story group's enemy
 * ranges, so the same tables are used with group 0. His blades' classes and
 * affinities are never below 1 unbalanced there (1-255); that difference is
 * dropped. Starting gear is in inventory format. */
enum {
    INV_BLADE_CLASSES = 0x18,
    INV_BLADE_AFFINITIES = 0x20,
    INV_GRIP_BEP = 0x09,
    INV_ARMOR_BEP = 0x11,
    INV_ARMOR_CLASSES = 0x14,
    INV_ARMOR_AFFINITIES = 0x1C,
};

static void ashleyRoller(roller_t* r, u32 part)
{
    rng_init(&r->rng, rando_config.seed, STREAM_ENEMY_GEAR | 0xFF0000 | part);
    r->group = rando_config.options & OPT_BALANCE_ASHLEY ? 0 : -1;
}

void rando_ashleyGearStats(void)
{
    u32 opts = rando_config.options2;
    roller_t r;
    int i;

    if (opts & OPT2_OPEN_WORLD) {
        rando_openWorld();
    }
    ashleyRoller(&r, 0);
    for (i = 0; i < 16; ++i) {
        u8* b = (u8*)&vs_main_inventory.blades[i];
        u8* g = (u8*)&vs_main_inventory.grips[i];
        u8* a = (u8*)&vs_main_inventory.armor[i];
        if (vs_main_inventory.blades[i].id != 0) {
            if (opts & OPT2_FANDANGO_COST) {
                rando_rollCostType(&r.rng, &vs_main_inventory.blades[i]);
            }
            if (opts & OPT2_ASHLEY_CLASS) {
                rollBytes(&r, b + INV_BLADE_CLASSES, CLASS_COUNT, &bladeClass);
            }
            if (opts & OPT2_ASHLEY_AFFINITY) {
                rollBytes(&r, b + INV_BLADE_AFFINITIES, AFFINITY_COUNT, &bladeAffinity);
            }
        }
        if (vs_main_inventory.grips[i].id != 0 && (opts & OPT2_ASHLEY_BEP)) {
            rollBytes(&r, g + INV_GRIP_BEP, 3, &gripBep);
        }
        if (vs_main_inventory.armor[i].id != 0) {
            int accessory = vs_main_inventory.armor[i].category == ARMOR_ACCESSORY;
            if (opts & OPT2_ASHLEY_BEP) {
                rollBytes(&r, a + INV_ARMOR_BEP, 3, accessory ? &accessoryBep : &armorBep);
            }
            if (opts & OPT2_ASHLEY_CLASS) {
                rollBytes(&r, a + INV_ARMOR_CLASSES, CLASS_COUNT, accessory ? &accessoryClass : &armorClass);
            }
            if (opts & OPT2_ASHLEY_AFFINITY) {
                rollBytes(&r, a + INV_ARMOR_AFFINITIES, AFFINITY_COUNT, accessory ? &accessoryAffinity : &armorAffinity);
            }
        }
    }
}

/* His body parts' BEP and affinities, in the template INITBTL builds him
 * from (reloaded from disc every time, so this runs every time too). */
void rando_ashleyBody(vs_zndEnemy* ashley)
{
    u32 opts = rando_config.options2;
    u8* limb = (u8*)ashley + T_LIMBS;
    roller_t r;
    int i;

    ashleyRoller(&r, 1);
    for (i = 0; i < 6; ++i, limb += LIMB_SIZE) {
        if (get16(limb + LIMB_HP) == 0) {
            continue;
        }
        if (opts & OPT2_ASHLEY_BEP) {
            rollBytes(&r, limb + LIMB_BEP, 3, &limbBep);
        }
        if (opts & OPT2_ASHLEY_AFFINITY) {
            rollBytes(&r, limb + LIMB_AFFINITIES, AFFINITY_COUNT, &limbAffinity);
        }
    }
}
