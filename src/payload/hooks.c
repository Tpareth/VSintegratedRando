/* Wrappers installed over calls in the game (see tools/build.py). Each one
 * randomizes freshly loaded data in place, then runs the original function. */
#include "rando.h"

/* Buttons held down on top of the real controller; written by the test
 * harness over GDB (tools/emu.py) to drive the game. */
u32 rando_debugButtons;

/* Replaces `jal vs_main_updatePadState` for port 0 at 0x80043960. */
u32 rando_onPadRead(int port, void* buffer)
{
    return vs_main_updatePadState(port, buffer) | rando_debugButtons;
}

/* Replaces `jal _loadMpdRoomSection` at 0x80089E80, the first parse of an MPD
 * after it finishes loading. _zoneContext.mpdData holds the whole file. */
void rando_onMpdLoaded(int index, void* room)
{
    vs_mpdHeader* mpd = _zoneContext.mpdData;
    u32 roomKey = (D_800F1AB0.zndId << 8) | D_800F1AB0.mpdId;

    if (rando_enabled()) {
        if (mpd->treasure.length == sizeof(vs_loot)) {
            vs_loot* loot = (vs_loot*)((u8*)mpd + mpd->treasure.offset);
            if (rando_config.options & OPT_CHESTS) {
                rando_chests(loot, roomKey);
            }
            if (rando_config.options & OPT_CHEST_GEAR) {
                rando_chestGear(loot, roomKey);
            }
            if (rando_overlay.magic == RANDO_OVERLAY_MAGIC) {
                rando_overlay.chestStats(loot, roomKey);
            }
        }
        if (rando_overlay.magic == RANDO_OVERLAY_MAGIC) {
            rando_overlay.room(mpd, roomKey);
        }
        rando_zenithBossDrops((vs_mpdEnemy*)((u8*)mpd + mpd->enemy.offset),
            mpd->enemy.length / sizeof(vs_mpdEnemy), roomKey);
        if (rando_config.options & OPT_ENEMY_DROPS) {
            rando_mpdDrops((vs_mpdEnemy*)((u8*)mpd + mpd->enemy.offset),
                mpd->enemy.length / sizeof(vs_mpdEnemy), D_800F1AB0.zndId, roomKey);
        }
    }
    _loadMpdRoomSection(index, room);
}

/* Replaces `jal func_80076F24(0, &D_800FAB18, ...)` at 0x800F9E2C in
 * INITBTL.PRG, which creates Ashley from his template every time the battle
 * program starts (a save then overrides his stats on Continue). */
int rando_onAshleyInit(int slot, vs_zndEnemy* init, int a2, int a3, int a4, int a5)
{
    rando_overlay.magic = 0;
    if (rando_enabled() && rando_overlayFile.size != 0) {
        rando_loadFile(&rando_overlayFile, (void*)RANDO_OVERLAY_ADDR);
    }
    if (rando_overlay.magic == RANDO_OVERLAY_MAGIC) {
        if (rando_newGameGear) {
            rando_overlay.ashleyGear();
        }
        rando_overlay.ashleyBody(init);
    }
    rando_newGameGear = 0;
    rando_abilities(); /* also restores vanilla tables for vanilla games */
    rando_zenithAnalyze();
    rando_zenithBattle();
    if (rando_enabled()) {
        int build = rando_build();
        if (build) {
            rando_buildStats(init, build);
        } else if (rando_config.options & OPT_ASHLEY_STATS) {
            rando_ashleyStats(init);
        }
    }
    return func_80076F24(slot, init, a2, a3, a4, a5);
}

/* Replaces `jal _loadZndEnemies` at 0x80088FA8; data is the ZND enemy
 * section while the raw ZND buffer is still live. */
void rando_onZndEnemies(void* data, int len)
{
    if (rando_enabled()) {
        rando_zenithQuiet(data, D_800F1AB0.zndId);
        if (rando_config.options & OPT_ENEMY_STATS) {
            rando_zndEnemies(data, D_800F1AB0.zndId); /* first: appendage HP scales with enemy HP */
        }
        if (rando_overlay.magic == RANDO_OVERLAY_MAGIC) {
            rando_overlay.enemyGear(data, D_800F1AB0.zndId);
        }
    }
    _loadZndEnemies(data, len);
}
