/* Picking random real equipment from the generated pools. maxMaterial caps
 * the material tier (MATERIAL_ANY for no cap); accessories and gems have no
 * material and are never capped. */
#include "equipment.h"
#include "rando.h"

/* Blade category (1 dagger .. 9 crossbow) to the grip category it takes:
 * guards, grips, poles, bolts/bullets. */
static const u8 gripForBlade[10] = { 0, 1, 1, 1, 2, 2, 2, 2, 3, 4 };

int gear_gripCategoryFor(int bladeCategory)
{
    return bladeCategory >= 0 && bladeCategory < 10 ? gripForBlade[bladeCategory] : 0;
}

typedef int (*accept_t)(const u8* record, int arg);

/* Uniform pick among the records accept() allows; -1 if there are none. */
static int pick(rng_t* rng, const u8* pool, int count, int size, accept_t accept, int arg)
{
    int matches = 0;
    int n;
    int i;

    for (i = 0; i < count; ++i) {
        matches += accept(pool + i * size, arg);
    }
    if (matches == 0) {
        return -1;
    }
    n = rng_range(rng, 0, matches - 1);
    for (i = 0; i < count; ++i) {
        if (accept(pool + i * size, arg) && n-- == 0) {
            return i;
        }
    }
    return -1;
}

static int bladeAccept(const u8* r, int maxMaterial)
{
    return ((const vs_blade*)r)->material <= maxMaterial;
}

static int gripAccept(const u8* r, int category)
{
    return ((const vs_grip*)r)->category == category;
}

static int shieldAccept(const u8* r, int maxMaterial)
{
    return ((const vs_armor*)r)->material <= maxMaterial;
}

/* arg = category | maxMaterial << 8 */
static int armorAccept(const u8* r, int arg)
{
    const vs_armor* a = (const vs_armor*)r;
    return a->category == (arg & 0xFF) && (a->category == ARMOR_ACCESSORY || a->material <= (arg >> 8));
}

static int anyAccept(const u8* r, int arg)
{
    return 1;
}

const vs_blade* gear_findBlade(int id)
{
    int i;

    for (i = 0; i < pool_bladeCount; ++i) {
        if (((const vs_blade*)pool_blades[i])->id == id) {
            return (const vs_blade*)pool_blades[i];
        }
    }
    return NULL;
}

const vs_grip* gear_findGrip(int id)
{
    int i;

    for (i = 0; i < pool_gripCount; ++i) {
        if (((const vs_grip*)pool_grips[i])->id == id) {
            return (const vs_grip*)pool_grips[i];
        }
    }
    return NULL;
}

const vs_blade* gear_pickBlade(rng_t* rng, int maxMaterial)
{
    int i = pick(rng, pool_blades[0], pool_bladeCount, sizeof pool_blades[0], bladeAccept, maxMaterial);
    return i < 0 ? NULL : (const vs_blade*)pool_blades[i];
}

const vs_grip* gear_pickGrip(rng_t* rng, int category)
{
    int i = pick(rng, pool_grips[0], pool_gripCount, sizeof pool_grips[0], gripAccept, category);
    return i < 0 ? NULL : (const vs_grip*)pool_grips[i];
}

const vs_armor* gear_pickShield(rng_t* rng, int maxMaterial)
{
    int i = pick(rng, pool_shields[0], pool_shieldCount, sizeof pool_shields[0], shieldAccept, maxMaterial);
    return i < 0 ? NULL : (const vs_armor*)pool_shields[i];
}

const vs_armor* gear_pickArmor(rng_t* rng, int category, int maxMaterial)
{
    int i = pick(rng, pool_armor[0], pool_armorCount, sizeof pool_armor[0], armorAccept,
        category | (maxMaterial << 8));
    return i < 0 ? NULL : (const vs_armor*)pool_armor[i];
}

const vs_gem* gear_pickGem(rng_t* rng)
{
    int i = pick(rng, pool_gems[0], pool_gemCount, sizeof pool_gems[0], anyAccept, 0);
    return i < 0 ? NULL : (const vs_gem*)pool_gems[i];
}
