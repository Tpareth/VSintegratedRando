/* Host-side harness for the payload randomizers. Reads records from stdin,
 * each a header {u32 kind, u32 a, u32 b, u32 len} followed by len bytes, runs
 * the matching randomizer in place and writes the bytes back to stdout.
 * Driven by tests/test_rando.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/payload/rando.h"

rando_config_t rando_config = { RANDO_CONFIG_MAGIC };
vs_inventory vs_main_inventory;

void vs_main_memcpy(void* dst, const void* src, u32 size)
{
    memcpy(dst, src, size);
}

void vs_main_bzero(void* dst, u32 size)
{
    memset(dst, 0, size);
}

/* Ability tables (SLUS_010.40 data) and the BATTLE.PRG frame-window code. */
vs_action_t vs_main_actions[224];
u16 vs_main_artsPointsRequirements[40];
u16 vs_main_battleAbilitiesPointsRequirements[22];
static u32 frameCode[2];
u32* const frameWindowCode[2] = { &frameCode[0], &frameCode[1] };

void FlushCache(void) { }

/* Game and payload code zenith.c refers to but the host never runs. */
int menu0_teleportMp;
void menu0_setMpCost(int cost, int insufficient) { }
void rando_dropStubWeapon(void) { }
void rando_dropStubShield(void) { }
void rando_dropStubAccessory(void) { }
void rando_dropStubArmor(void) { }

/* KIND_DROP_ROLLS record: a sequence of rando_dropRoll calls. */
typedef struct {
    u8 roll, rate, kind, category;
    u16 id, result;
} dropRoll_t;

/* KIND_ABILITIES record: the tables above, in this order. */
static void* const abilityParts[] = { vs_main_actions, vs_main_artsPointsRequirements,
    vs_main_battleAbilitiesPointsRequirements, frameCode };
static const u32 abilitySizes[] = { sizeof vs_main_actions, sizeof vs_main_artsPointsRequirements,
    sizeof vs_main_battleAbilitiesPointsRequirements, sizeof frameCode };

enum { KIND_CHESTS = 1, KIND_ZND_ENEMIES = 2, KIND_MPD_DROPS = 3, KIND_ASHLEY_STATS = 4, KIND_ASHLEY_GEAR = 5,
    KIND_ABILITIES = 6, KIND_DROP_ROLLS = 7, KIND_ROOM = 8 };

int main(int argc, char** argv)
{
    u32 hdr[4];
    u8* buf;

    if (argc != 3) {
        fprintf(stderr, "usage: %s seed options\n", argv[0]);
        return 2;
    }
    /* The real config code: catches checksum bugs that would silently turn a
     * randomized game vanilla. */
    {
        unsigned long long options = strtoull(argv[2], NULL, 0);
        rando_setConfig(strtoul(argv[1], NULL, 0), (u32)options, (u32)(options >> 32));
    }
    if (!rando_enabled()) {
        fprintf(stderr, "rando_enabled() rejects the config rando_setConfig() wrote\n");
        return 3;
    }
    while (fread(hdr, sizeof hdr, 1, stdin) == 1) {
        buf = malloc(hdr[3]);
        if (fread(buf, 1, hdr[3], stdin) != hdr[3]) {
            return 1;
        }
        /* Option gating mirrors src/payload/hooks.c. */
        switch (hdr[0]) {
        case KIND_CHESTS:
            if (rando_config.options & OPT_CHESTS) {
                rando_chests((vs_loot*)buf, hdr[1]);
            }
            if (rando_config.options & OPT_CHEST_GEAR) {
                rando_chestGear((vs_loot*)buf, hdr[1]);
            }
            rando_chestStats((vs_loot*)buf, hdr[1]); /* the battle overlay */
            break;
        case KIND_ZND_ENEMIES:
            rando_zenithQuiet(buf, hdr[1]);
            if (rando_config.options & OPT_ENEMY_STATS) {
                rando_zndEnemies(buf, hdr[1]);
            }
            rando_enemyGear(buf, hdr[1]); /* the battle overlay */
            break;
        case KIND_MPD_DROPS:
            rando_zenithBossDrops((vs_mpdEnemy*)buf, hdr[3] / sizeof(vs_mpdEnemy), hdr[2]);
            if (rando_config.options & OPT_ENEMY_DROPS) {
                rando_mpdDrops((vs_mpdEnemy*)buf, hdr[3] / sizeof(vs_mpdEnemy), hdr[1], hdr[2]);
            }
            break;
        case KIND_ASHLEY_STATS:
            rando_ashleyBody((vs_zndEnemy*)buf);
            if (rando_build()) {
                rando_buildStats((vs_zndEnemy*)buf, rando_build());
            } else if (rando_config.options & OPT_ASHLEY_STATS) {
                rando_ashleyStats((vs_zndEnemy*)buf);
            }
            break;
        case KIND_ABILITIES: {
            u8* p = buf;
            int i;
            for (i = 0; i < 4; p += abilitySizes[i], ++i) {
                memcpy(abilityParts[i], p, abilitySizes[i]);
            }
            rando_abilities();
            for (p = buf, i = 0; i < 4; p += abilitySizes[i], ++i) {
                memcpy(p, abilityParts[i], abilitySizes[i]);
            }
            break;
        }
        case KIND_DROP_ROLLS: {
            dropRoll_t* r = (dropRoll_t*)buf;
            u32 i;
            for (i = 0; i < hdr[3] / sizeof *r; ++i) {
                u16 item[3] = { r[i].id, 0, r[i].category }; /* id, then the armor category at byte 4 */
                r[i].result = rando_dropRoll(r[i].roll, r[i].rate, item, r[i].kind);
            }
            break;
        }
        case KIND_ROOM: /* a whole MPD file; a = roomKey */
            rando_roomPatches((vs_mpdHeader*)buf, hdr[1]);
            break;
        case KIND_ASHLEY_GEAR:
            memcpy(&vs_main_inventory, buf, sizeof vs_main_inventory);
            rando_ashleyGear();
            rando_ashleyGearStats(); /* the battle overlay, at the first battle start */
            memcpy(buf, &vs_main_inventory, sizeof vs_main_inventory);
            break;
        }
        fwrite(buf, 1, hdr[3], stdout);
        free(buf);
    }
    return 0;
}
