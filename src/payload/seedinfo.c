/* Shows the seed and preset on the in-game main menu (Triangle), Vagrant
 * Story's equivalent of a pause screen, so a run can be set up again. */
#include "rando.h"

/* Under the command list; the menu font has upper case, digits and
 * punctuation only. */
#define TEXT_X 196
#define TEXT_Y 150
#define LINE_H 12

static const char* presetName(u32 options, u32 options2)
{
    if (options == PRESET_STANDARD && options2 == PRESET2_STANDARD) {
        return "STANDARD";
    }
    if (options == PRESET_CHAOS && options2 == PRESET2_CHAOS) {
        return "CHAOS";
    }
    return "CUSTOM";
}

static char* append(char* p, const char* s)
{
    while (*s) {
        *p++ = *s++;
    }
    return p;
}

/* Replaces `jal vs_mainMenu_exec` at 0x800C9840 (BATTLE.PRG), called every
 * frame while the main menu or one of its submenus is open. */
void rando_onMainMenu(int menu)
{
    char text[16]; /* "SEED " + 8 digits */
    char* p;
    u32 seed;
    int i;

    vs_mainMenu_exec(menu);

    /* Top-level menu only; submenus use the whole screen. */
    if ((menu & 0x3F) != 0 || !rando_enabled()) {
        return;
    }
    p = append(text, "SEED ");
    seed = rando_config.seed;
    for (i = 7; i >= 0; --i) {
        p[i] = '0' + seed % 10;
        seed /= 10;
    }
    p[8] = 0;
    vs_battle_renderTextRaw(text, (TEXT_Y << 16) | TEXT_X, NULL);
    vs_battle_renderTextRaw(presetName(rando_config.options, rando_config.options2), ((TEXT_Y + LINE_H) << 16) | TEXT_X, NULL);
}
