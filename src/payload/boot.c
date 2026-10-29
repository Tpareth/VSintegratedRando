/* Boot-time options menu. The menu is its own binary (src/menu) stored in
 * free disc sectors; on first boot it is loaded into overlay slot 0, which
 * TITLE.PRG has not been loaded into yet, and run before the title screen. The
 * chosen settings wait in rando_pending until New Game is picked. */
#include "rando.h"

#define MENU_ADDR ((void*)0x80068800)

/* Locations of the menu and battle overlay binaries; filled in by
 * tools/build.py. */
rando_file_t rando_menuFile = { RANDO_FILE_MAGIC, 0, 0 };
rando_file_t rando_overlayFile = { RANDO_FILE_MAGIC, 0, 0 };

rando_pending_t rando_pending;
int rando_newGameGear;

static int menuShown;

/* Blocking load of whole sectors through the game's CD queue. */
int rando_loadFile(const rando_file_t* file, void* dest)
{
    vs_main_CdFile cd = { file->lba, file->size };
    vs_main_CdQueueSlot* slot = vs_main_allocateCdQueueSlot(&cd);

    if (slot == NULL) {
        return 0;
    }
    vs_main_cdEnqueuePriority(slot, dest);
    while (slot->state != CD_QUEUE_LOADED) {
        vs_main_gametimeUpdate(0);
    }
    vs_main_freeCdQueueSlot(slot);
    FlushCache();
    return 1;
}

/* TITLE.PRG's New Game hook is installed in RAM each time the title loads
 * rather than on disc, so the disc file stays vanilla: other patches that
 * edit TITLE.PRG (the Rood Reverse QoL mod) then keep valid sector
 * checksums under ours. `selectedOption = _nop()` (0x800715A4) becomes
 * `jal rando_onNewGame`. */
#define TITLE_NEW_GAME_CALL ((u32*)0x800715A4)
#define TITLE_NOP 0x8007124C
#define JAL(target) (0x0C000000 | (((u32)(target) >> 2) & 0x03FFFFFF))

static void hookNewGame(void)
{
    if (*TITLE_NEW_GAME_CALL == JAL(TITLE_NOP)) {
        *TITLE_NEW_GAME_CALL = JAL(rando_onNewGame);
        FlushCache();
    }
}

/* Replaces `jal _loadTitlePrg` at 0x80042BD0 in vs_main_execTitle, right
 * after _sysReinit has set up the GPU, heap and CD queue. */
void rando_beforeTitle(void)
{
    if (!menuShown && rando_menuFile.size != 0) {
        menuShown = 1;
        if (rando_loadFile(&rando_menuFile, MENU_ADDR)) {
            ((void (*)(void))MENU_ADDR)();
        }
    }
    _loadTitlePrg();
    hookNewGame();
}

/* Replaces TITLE.PRG's `selectedOption = _nop()` (0x800715A4) when New Game
 * is chosen; the title has already reset the save flags and copied the
 * starting inventory, so this is where the config goes in. */
int rando_onNewGame(void)
{
    if (rando_pending.options != 0 || rando_pending.options2 != 0) {
        rando_setConfig(rando_pending.seed, rando_pending.options, rando_pending.options2);
        rando_ashleyGear();
        rando_newGameGear = 1;
    }
    return 0; /* menuItemNewGame */
}
