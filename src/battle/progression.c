/* Open World and Mandatory Iron Maiden (ChaoticBrave's Rooms::openWorld and
 * Rooms::mandyMaiden). */
#include "rando.h"

#define KEY_FIRST 0x1CA /* Bronze Key .. */
#define KEY_LAST 0x1E9 /* .. Mandrake Sigil: every key and sigil */
#define TIGERTAIL_SIGIL 0x1E6
#define GOLD_KEY 0x1CD
#define PLATINUM_KEY 0x1CE

/* Open World: every key and sigil in the starting inventory. The PC
 * randomizer writes them over the first 32 misc slots, starting items
 * included; here they go into the empty slots. Called with the other
 * once-per-New-Game inventory changes. */
void rando_openWorld(void)
{
    int id = KEY_FIRST;
    int i;

    for (i = 0; i < 64 && id <= KEY_LAST; ++i) {
        vs_inventoryMisc* m = &vs_main_inventory.misc[i];
        if (m->id == 0) {
            m->id = id++;
            m->count = 1;
        }
    }
}

/* Mandatory Iron Maiden: the two doors of MAP171 (Great Cathedral, room
 * 0x2700) are locked with the Tigertail Sigil, and Kali (MAP145, room 0x1F00,
 * spawn 0) drops the Gold and Platinum Keys. Only applied to the vanilla
 * data. */
#define CATHEDRAL_ROOM 0x2700
#define KALI_ROOM 0x1F00
#define SECTION_A 10 /* index in the room section's length table */
#define LOCK_SIZE 0x14
#define LOCK_STATE 0x08
#define LOCK_KEY 0x0C /* s16 keyOrSigil */

static void lockCathedral(vs_mpdHeader* mpd)
{
    u8* room = (u8*)mpd + mpd->room.offset;
    const u32* lengths = (const u32*)room;
    u8* lock = room + 24 * 4;
    int i;

    for (i = 0; i < SECTION_A - 1; ++i) {
        lock += lengths[i];
    }
    if (lengths[SECTION_A - 1] != 2 * LOCK_SIZE) {
        return;
    }
    for (i = 0; i < 2; ++i) {
        if (lock[i * LOCK_SIZE + LOCK_KEY] | lock[i * LOCK_SIZE + LOCK_KEY + 1]) {
            return;
        }
    }
    for (i = 0; i < 2; ++i, lock += LOCK_SIZE) {
        lock[LOCK_STATE] = 1;
        lock[LOCK_KEY] = TIGERTAIL_SIGIL & 0xFF;
        lock[LOCK_KEY + 1] = TIGERTAIL_SIGIL >> 8;
    }
}

static void kaliKeys(vs_mpdHeader* mpd)
{
    vs_mpdEnemy* kali = (vs_mpdEnemy*)((u8*)mpd + mpd->enemy.offset);

    if (mpd->enemy.length < sizeof *kali || kali->type != 0 || kali->itemId != 0 || kali->item2Id != 0) {
        return;
    }
    kali->itemId = GOLD_KEY;
    kali->item2Id = PLATINUM_KEY;
    kali->itemCount = 1;
    kali->item2Count = 1;
}

void rando_roomPatches(vs_mpdHeader* mpd, u32 roomKey)
{
    if (!(rando_config.options2 & OPT2_IRON_MAIDEN)) {
        return;
    }
    if (roomKey == CATHEDRAL_ROOM) {
        lockCathedral(mpd);
    } else if (roomKey == KALI_ROOM) {
        kaliKeys(mpd);
    }
}
