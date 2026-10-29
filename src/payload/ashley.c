/* Ashley randomization (port of ChaoticBrave's Ashley.cpp): base stats, and
 * the starting weapon, armor and items. */
#include "rando.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

enum { STREAM_PART_STATS, STREAM_PART_WEAPON, STREAM_PART_ARMOR, STREAM_PART_ITEMS };

static void rngFor(rng_t* rng, int part)
{
    rng_init(rng, rando_config.seed, STREAM_ASHLEY | part);
}

static int balanced(void)
{
    return rando_config.options & OPT_BALANCE_ASHLEY;
}

static int maxMaterial(void)
{
    return balanced() ? MATERIAL_IRON : MATERIAL_ANY;
}

/* --- base stats ----------------------------------------------------------- */

/* Ranges from Ashley.cpp. The balanced STR/INT/AGL range is its threshold
 * (low 75-125, medium 50-150, high 25-175), applied to all three stats as
 * intended (the original only assigned it to AGL). */
static const u8 thresholdSpread[4] = { 50, 25, 75, 50 };

void rando_ashleyStats(vs_zndEnemy* ashley)
{
    rng_t rng;

    rngFor(&rng, STREAM_PART_STATS);
    if (balanced()) {
        int spread = thresholdSpread[(rando_config.options2 & OPT2_THRESHOLD_MASK) >> OPT2_THRESHOLD_SHIFT];
        ashley->hp = rng_range(&rng, 40, 480);
        ashley->mp = rng_range(&rng, 5, 130);
        ashley->strength = rng_range(&rng, 100 - spread, 100 + spread);
        ashley->intelligence = rng_range(&rng, 100 - spread, 100 + spread);
        ashley->agility = rng_range(&rng, 100 - spread, 100 + spread);
    } else {
        ashley->hp = rng_range(&rng, 40, 999);
        ashley->mp = rng_range(&rng, 5, 999);
        ashley->strength = rng_range(&rng, 1, 255);
        ashley->intelligence = rng_range(&rng, 1, 255);
        ashley->agility = rng_range(&rng, 1, 255);
    }
    ashley->walkSpeed = rng_range(&rng, 10, 14);
    ashley->runSpeed = rng_range(&rng, 20, 28);
}

/* --- starting equipment --------------------------------------------------- */

/* Replace the blade and grip of the starting weapon (the Fandango). The
 * weapon keeps its slot, name and gem settings. */
static void randomizeWeapon(vs_inventory* inv, rng_t* rng)
{
    vs_inventoryWeapon* w = &inv->weapons[0];
    const vs_blade* newBlade;
    const vs_grip* newGrip;
    vs_blade* blade;
    vs_grip* grip;

    if (w->blade == 0 || w->grip == 0 || (newBlade = gear_pickBlade(rng, maxMaterial())) == NULL) {
        return;
    }
    blade = &inv->blades[w->blade - 1];
    grip = &inv->grips[w->grip - 1];

    vs_main_memcpy(blade, newBlade, sizeof *blade);
    blade->assembledWeaponIndex = w->index;
    blade->index = w->blade;

    newGrip = gear_pickGrip(rng, gear_gripCategoryFor(blade->category));
    if (newGrip != NULL) {
        vs_main_memcpy(grip, newGrip, sizeof *grip);
    }
    grip->assembledWeaponIndex = w->index;
    grip->index = w->grip;
}

/* Replace every starting armor piece with one for the same body slot,
 * keeping which limb it is equipped on. */
static void randomizeArmor(vs_inventory* inv, rng_t* rng)
{
    unsigned i;

    for (i = 0; i < COUNT(inv->armor); ++i) {
        vs_armor* a = &inv->armor[i];
        const vs_armor* piece;
        u8 limb;
        u8 index;

        if (a->id == 0 || (piece = gear_pickArmor(rng, a->category, maxMaterial())) == NULL) {
            continue;
        }
        limb = a->limb;
        index = a->index;
        vs_main_memcpy(a, piece, sizeof *a);
        a->limb = limb;
        a->index = index;
    }
}

/* Replace the starting misc items with consumables or grimoires. Counts stay
 * vanilla when quantities are balanced. */
static void randomizeItems(vs_inventory* inv, rng_t* rng)
{
    unsigned i;

    for (i = 0; i < COUNT(inv->misc); ++i) {
        vs_inventoryMisc* m = &inv->misc[i];
        if (m->id == 0) {
            continue;
        }
        m->id = item_randomMisc(rng);
        if (item_isGrimoire(m->id)) {
            m->count = 1;
        } else if (!(rando_config.options & OPT_BALANCE_QUANTITIES)) {
            m->count = rng_range(rng, 1, 99);
        } else if (m->count == 0) {
            m->count = 1;
        }
    }
}

/* Called once the New Game menu confirms a config: TITLE.PRG has already
 * copied the starting inventory into vs_main_inventory. */
void rando_ashleyGear(void)
{
    int build = rando_build();
    rng_t rng;

    /* A build sets the weapon and items itself. */
    if (build) {
        rando_buildGear(&vs_main_inventory, build);
    } else if (rando_config.options & OPT_ASHLEY_WEAPON) {
        rngFor(&rng, STREAM_PART_WEAPON);
        randomizeWeapon(&vs_main_inventory, &rng);
    }
    if (rando_config.options & OPT_ASHLEY_ARMOR) {
        rngFor(&rng, STREAM_PART_ARMOR);
        randomizeArmor(&vs_main_inventory, &rng);
    }
    if (!build && (rando_config.options & OPT_ASHLEY_ITEMS)) {
        rngFor(&rng, STREAM_PART_ITEMS);
        randomizeItems(&vs_main_inventory, &rng);
    }
}
