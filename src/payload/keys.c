/* Loot keys/sigils, ported from Chests.cpp and Enemies.cpp. Random items come
 * uniformly from consumables, grimoires, keys and sigils (as item ids 67-233
 * minus the "untitled" ones). Bronze, Silver and Iron Keys are constrained by
 * what earlier rooms rolled, walking rooms in story order: the first of them
 * is always Bronze, Silver comes before Iron, and later ones are limited to
 * certain areas. Each room is randomized on its own when it loads, so the
 * draws of all earlier rooms are replayed from the seed. */
#include "keys.h"
#include "rando.h"

#define BRONZE_KEY 0x1CA
#define IRON_KEY 0x1CB
#define SILVER_KEY 0x1CC
#define CHAMOMILE_SIGIL 0x1D2
#define CURE_ROOT 0x143

#define KEY_COUNT (ITEM_SIGIL_LAST - ITEM_KEY_FIRST + 1) /* keys and sigils */

typedef struct {
    int bronzeArea; /* -1 until found */
    int silverArea;
} keyState_t;

/* Uniform over the 116 valid misc items, as dist_20 with its rerolls. */
static u16 drawItem(rng_t* rng, const keyRoom_t* room)
{
    u16 id;

    do {
        int pick = rng_range(rng, 0, ITEM_CONSUMABLE_COUNT + item_grimoireCount + KEY_COUNT - 1);
        if (pick < ITEM_CONSUMABLE_COUNT) {
            id = ITEM_CONSUMABLE_FIRST + pick;
        } else if ((pick -= ITEM_CONSUMABLE_COUNT) < item_grimoireCount) {
            id = item_grimoireAt(pick);
        } else {
            id = ITEM_KEY_FIRST + pick - item_grimoireCount;
        }
    } while (id == CHAMOMILE_SIGIL && room->preMinotaur);
    return id;
}

/* The area-order rule from Chests.cpp: once a key was found in one of these
 * areas, the next key may only be found in areas up to that one in this
 * sequence (the last three areas share a rank). */
static int keyRank(int area)
{
    static const s8 ranks[21] = {
        -1, -1, -1, -1, -1, -1, -1, -1, 0, 4, 1, 5, -1, 5, 3, -1, 5, -1, 2, -1, -1,
    };
    return area >= 0 && area < 21 ? ranks[area] : -1;
}

static int outOfOrder(int foundArea, int area)
{
    int found = keyRank(foundArea);
    int here = keyRank(area);
    return found >= 0 && (here < 0 || here > found);
}

static u16 applyKeyOrder(u16 id, keyState_t* s, int area)
{
    if (id == BRONZE_KEY) {
        s->bronzeArea = area;
    } else if (id == SILVER_KEY || id == IRON_KEY) {
        if (s->bronzeArea < 0) {
            id = BRONZE_KEY;
            s->bronzeArea = area;
        } else if (outOfOrder(s->bronzeArea, area)) {
            id = CURE_ROOT;
        } else if (id == SILVER_KEY) {
            s->silverArea = area;
        } else if (s->silverArea < 0) {
            id = SILVER_KEY;
            s->silverArea = area;
        } else if (outOfOrder(s->silverArea, area)) {
            id = CURE_ROOT;
        }
    }
    return id;
}

static int drawCount(const keyRoom_t* r, int pass)
{
    return pass == KEY_PASS_CHESTS ? 3 : r->drops;
}

static int eligible(const keyRoom_t* r, int pass, int draw)
{
    return pass == KEY_PASS_CHESTS ? (r->chestSlots >> draw) & 1 : 1;
}

int keys_roomArea(u32 roomKey)
{
    int i;

    for (i = 0; i < keyRoomCount; ++i) {
        if (keyRooms[i].roomKey == roomKey) {
            return keyRooms[i].area;
        }
    }
    return -1;
}

int keys_roomItems(u32 roomKey, int pass, u16 out[KEY_MAX_DRAWS])
{
    keyState_t state = { -1, -1 };
    rng_t rng;
    int i;
    int d;

    for (i = 0; i < keyRoomCount; ++i) {
        const keyRoom_t* r = &keyRooms[i];
        for (d = 0; d < drawCount(r, pass) && d < KEY_MAX_DRAWS; ++d) {
            u16 id;
            if (!eligible(r, pass, d)) {
                continue;
            }
            rng_init(&rng, rando_config.seed, STREAM_LOOT_KEYS | (pass << 20) | (r->roomKey << 4) | d);
            id = applyKeyOrder(drawItem(&rng, r), &state, r->area);
            if (r->roomKey == roomKey) {
                out[d] = id;
            }
        }
        if (r->roomKey == roomKey) {
            return 1;
        }
    }
    return 0;
}
