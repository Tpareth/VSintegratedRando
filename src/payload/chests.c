/* Chest content randomization (after ChaoticBrave's Chests.cpp): misc items
 * (consumables and grimoires, plus keys/sigils with loot keys on) and
 * equipment. Vanilla keys and sigils are never touched. */
#include "keys.h"
#include "rando.h"

static void randomizeMisc(vs_lootMisc* m, rng_t* rng)
{
    if (m->difficultyFlags == 0) {
        return;
    }
    if (item_isConsumable(m->id)) {
        m->id = item_randomConsumable(rng);
        if (rando_config.options & OPT_BALANCE_QUANTITIES) {
            m->count = rng_range(rng, 1, 10);
        } else {
            m->count = rng_range(rng, 1, 100);
        }
    } else if (item_isGrimoire(m->id)) {
        m->id = item_randomGrimoire(rng);
        m->count = 1;
    }
}

/* With loot keys on, slots present on both difficulties take an item from the
 * shared key-aware draw instead (any misc item, keys and sigils included). */
static int lootKeyItem(vs_lootMisc* m, u16 id, rng_t* rng)
{
    if (id == 0 || m->difficultyFlags != 3 || item_isProgression(m->id)) {
        return 0;
    }
    m->id = id;
    if (!item_isConsumable(id)) {
        m->count = 1;
    } else if (rando_config.options & OPT_BALANCE_QUANTITIES) {
        m->count = rng_range(rng, 1, 10);
    } else {
        m->count = rng_range(rng, 1, 100);
    }
    return 1;
}

/* --- equipment --- */

/* Balanced material cap by story-ordered area: Iron through the Abandoned
 * Mines, Hagane to the Town and Undercity, Silver to the Snowfly Forest and
 * Keep, anything afterwards (and in rooms outside the area table). */
static int materialCap(u32 roomKey)
{
    int area;

    if (!(rando_config.options & OPT_BALANCE_QUANTITIES)) {
        return MATERIAL_ANY;
    }
    area = keys_roomArea(roomKey);
    if (area < 0) {
        return MATERIAL_ANY;
    }
    return area < 8 ? MATERIAL_IRON : area < 13 ? MATERIAL_HAGANE : area < 17 ? MATERIAL_SILVER : MATERIAL_ANY;
}

static void copy(void* dst, const void* src, u32 size)
{
    if (src != NULL) {
        vs_main_memcpy(dst, src, size);
    }
}

/* Each piece of equipment present in the chest becomes a random real item of
 * the same kind. Weapons keep their name and get a matching grip; weapon and
 * shield gem slots are left empty, as in every vanilla chest weapon. */
void rando_chestGear(vs_loot* loot, u32 roomKey)
{
    int cap = materialCap(roomKey);
    rng_t rng;
    int i;

    rng_init(&rng, rando_config.seed, STREAM_CHEST_GEAR | roomKey);
    if (loot->weapon.difficultyFlags) {
        copy(&loot->weapon.blade, gear_pickBlade(&rng, cap), sizeof loot->weapon.blade);
        copy(&loot->weapon.grip, gear_pickGrip(&rng, gear_gripCategoryFor(loot->weapon.blade.category)),
            sizeof loot->weapon.grip);
        vs_main_bzero(loot->weapon.gems, sizeof loot->weapon.gems);
    }
    if (loot->blade.difficultyFlags) {
        copy(&loot->blade.blade, gear_pickBlade(&rng, cap), sizeof loot->blade.blade);
    }
    if (loot->grip.difficultyFlags) {
        copy(&loot->grip.grip, gear_pickGrip(&rng, rng_range(&rng, 1, 4)), sizeof loot->grip.grip);
    }
    if (loot->shield.difficultyFlags) {
        copy(&loot->shield.shield, gear_pickShield(&rng, cap), sizeof loot->shield.shield);
        vs_main_bzero(loot->shield.gems, sizeof loot->shield.gems);
    }
    for (i = 0; i < 2; ++i) {
        if (loot->armor[i].difficultyFlags) {
            copy(&loot->armor[i].armor, gear_pickArmor(&rng, loot->armor[i].armor.category, cap),
                sizeof loot->armor[i].armor);
        }
    }
    if (loot->accessory.difficultyFlags) {
        copy(&loot->accessory.armor, gear_pickArmor(&rng, ARMOR_ACCESSORY, MATERIAL_ANY),
            sizeof loot->accessory.armor);
    }
    if (loot->gem.difficultyFlags) {
        copy(&loot->gem.gem, gear_pickGem(&rng), sizeof loot->gem.gem);
    }
}

/* --- misc items --- */

void rando_chests(vs_loot* loot, u32 roomKey)
{
    u16 keyItems[KEY_MAX_DRAWS] = { 0 };
    rng_t rng;
    int i;

    if (rando_config.options & OPT_LOOT_KEYS) {
        keys_roomItems(roomKey, KEY_PASS_CHESTS, keyItems);
    }
    rng_init(&rng, rando_config.seed, STREAM_CHESTS | roomKey);
    for (i = 0; i < 3; ++i) {
        if (!lootKeyItem(&loot->misc[i], keyItems[i], &rng)) {
            randomizeMisc(&loot->misc[i], &rng);
        }
    }
}
