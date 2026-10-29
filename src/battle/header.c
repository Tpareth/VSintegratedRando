/* Battle overlay header, at the start of the binary: the resident code finds
 * the overlay's functions through it (see rando_overlay_t in rando.h). */
#include "rando.h"

__attribute__((section(".header"), used)) const rando_overlay_t rando_overlayHeader = {
    RANDO_OVERLAY_MAGIC,
    rando_enemyGear,
    rando_chestStats,
    rando_ashleyGearStats,
    rando_ashleyBody,
    rando_roomPatches,
};
