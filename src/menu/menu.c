/* Randomizer options menu: a separate binary loaded at boot by the resident
 * payload (src/payload/boot.c) into overlay slot 0, before the title screen.
 * The chosen settings go to rando_pending and are applied at New Game.
 *
 * Laid out after MottZilla's SotN Integrated Randomizer: a short main menu
 * (seed, randomize, preset, options, memory card, vanilla), a seed keypad
 * screen and a scrolling options list with help text on the right. */
#include "card.h"
#include "gpu.h"
#include "rando.h"
#include "rng.h"

#define SCREEN_W 320
#define SCREEN_H 240
#define FONT_X 960
#define FONT_Y 256
#define TEXT_X 12
#define TEXT_Y 8
#define GLYPH_W 8
#define LINE_H 9
#define LAST_LINE 23 /* 24 lines fit the 224 displayed rows */
#define RIGHT_COLUMN 23 /* help text and button hints */
#define RIGHT_WIDTH 15

#define SEED_DIGITS 8
#define SEED_MAX 99999999u

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Both option words: options in the low half, options2 in the high half. */
typedef unsigned long long opts_t;
#define O2(bits) ((opts_t)(bits) << 32)

/* --- options ----------------------------------------------------------------- */

/* An option is an on/off bit, or, with values, a field of the options
 * (bit = its mask) cycling through the NULL-terminated value names. An entry
 * without a bit is a section header. */
typedef struct {
    const char* name;
    const char* help;
    opts_t bit;
    const char* const* values;
} option_t;

#define HEADER(name) { name, NULL, 0, NULL }

static const char* const thresholdNames[] = { "Medium", "Low", "High", NULL };

static const char* const buildNames[] = {
    "Off", "Dagger", "Great Sword", "Axe", "Mace", "Great Axe", "Great Mace", "Polearm", "Staff", "Crossbow",
    "Random", NULL,
};

static const option_t options[] = {
    HEADER("CHESTS"),
    { "Chest Items", "Shuffles the consumables and grimoires in chests. Keys and sigils stay put.", OPT_CHESTS },
    { "Chest Equipment", "Weapons, armor, shields and gems in chests become random real items of the same kind.",
        OPT_CHEST_GEAR },
    { "Chest Gear Stats", "Random DP and PP for chest weapons, shields and armor, and random gem STR/INT/AGL.",
        O2(OPT2_CHEST_GEAR_BASE) },
    { "Chest Gear Classes", "Randomizes the class values of equipment found in chests.", O2(OPT2_CHEST_GEAR_CLASS) },
    { "Chest Gear Affinity", "Randomizes the affinity values of equipment found in chests.",
        O2(OPT2_CHEST_GEAR_AFFINITY) },
    { "Chest Gear BEP", "Randomizes the blunt, edged and piercing values of chest grips, shields and armor.",
        O2(OPT2_CHEST_GEAR_BEP) },
    { "Blade Cost Stat", "Chest blades cost a random stat when used.", O2(OPT2_BLADE_COST) },
    { "Balance Item Stats", "Chest gear stat rolls stay small early on and widen later.",
        O2(OPT2_BALANCE_ITEM_STATS) },
    { "Balance Chests", "Vanilla-like item amounts, and chest gear materials that fit the point in the story.",
        OPT_BALANCE_QUANTITIES },
    HEADER("ENEMIES"),
    { "Enemy Stats", "Randomizes enemy HP, MP, STR, INT, AGL and movement speed.", OPT_ENEMY_STATS },
    { "Balance Enemy Stats", "Enemy stats grow with the story instead of up to 999/255 anywhere.",
        OPT_BALANCE_ENEMIES },
    { "Enemy Drops", "Randomizes each enemy's chance based drop and its rate. Guaranteed drops stay.",
        OPT_ENEMY_DROPS },
    { "Enemy Gear Stats", "Enemy weapons, shields, armor and accessories: item, STR/INT/AGL, DP/PP, material, drop rate.",
        O2(OPT2_ENEMY_GEAR_BASE) },
    { "Enemy Gear Classes", "Randomizes the class values of enemy equipment.", O2(OPT2_ENEMY_GEAR_CLASS) },
    { "Enemy Gear Affinity", "Randomizes the affinity values of enemy equipment.", O2(OPT2_ENEMY_GEAR_AFFINITY) },
    { "Enemy Gear BEP", "Randomizes the blunt, edged and piercing values of enemy gear.", O2(OPT2_ENEMY_GEAR_BEP) },
    { "Enemy Body Parts", "Randomizes the HP and defenses of each enemy body part.", O2(OPT2_ENEMY_APPENDAGES) },
    { "Ice Wolf", "City Walls Hellhounds breathe ice instead of fire.", O2(OPT2_ICE_WOLF) },
    HEADER("ASHLEY"),
    { "Ashley Build", "Start with a setup for one weapon type: stats, blade, grip and items. Left/Right to pick.",
        OPT_BUILD_MASK, buildNames },
    { "Ashley Stats", "Randomizes Ashley's starting HP, MP, STR, INT, AGL and speed.", OPT_ASHLEY_STATS },
    { "Starting Weapon", "Replaces the Fandango's blade with a random one and a matching grip.",
        OPT_ASHLEY_WEAPON },
    { "Starting Armor", "Replaces each starting armor piece and the accessory with a random one.",
        OPT_ASHLEY_ARMOR },
    { "Starting Items", "Replaces the starting items with random consumables and grimoires.", OPT_ASHLEY_ITEMS },
    { "Fandango Cost Stat", "The starting blade costs a random stat when used.", O2(OPT2_FANDANGO_COST) },
    { "Ashley Gear Classes", "Randomizes the class values of Ashley's starting equipment.", O2(OPT2_ASHLEY_CLASS) },
    { "Ashley Affinity", "Randomizes the affinities of Ashley's starting equipment and body.",
        O2(OPT2_ASHLEY_AFFINITY) },
    { "Ashley BEP", "Randomizes the blunt, edged and piercing values of Ashley's gear and body.",
        O2(OPT2_ASHLEY_BEP) },
    { "Balance Ashley", "Ashley's stats stay near vanilla and his gear is Bronze or Iron.", OPT_BALANCE_ASHLEY },
    { "Ashley Threshold", "Balanced STR/INT/AGL range: Low 75-125, Medium 50-150, High 25-175. Left/Right to pick.",
        O2(OPT2_THRESHOLD_MASK), thresholdNames },
    HEADER("ABILITIES"),
#if ZENITH_EDITION /* Zenith rearranges the action records, so only the learning kills (see rando.h) */
    { "Break Arts", "Randomizes the kills needed to learn each Break Art.", OPT_BREAK_ARTS },
    { "Balance Break Arts", "Keeps the kills needed (10-485) in the vanilla range.", OPT_BALANCE_BREAK_ARTS },
    { "Ability Learning", "Randomizes the chains needed to learn each battle ability (never above vanilla).",
        OPT_CHAIN_LEARNING },
    { "Ability Timing", "Randomizes the frames you have to time chain and defense abilities (8-30).",
        OPT_FRAME_WINDOWS },
#else
    { "Break Arts", "Randomizes the HP cost and the kills needed to learn each Break Art.", OPT_BREAK_ARTS },
    { "Balance Break Arts", "Keeps Break Art costs (25-75) and kills (10-485) in vanilla ranges.",
        OPT_BALANCE_BREAK_ARTS },
    { "Ability Learning", "Randomizes the chains needed to learn each battle ability (never above vanilla).",
        OPT_CHAIN_LEARNING },
    { "Ability Risk Costs", "Randomizes the risk cost of each chain and defense ability (1-6).", OPT_RISK_COSTS },
    { "Ultimate Spells", "The Lv.4 elemental spells become the enemies' ultimate spells.", OPT_ULTIMATE_SPELLS },
    { "Ability Timing", "Randomizes the frames you have to time chain and defense abilities (8-30).",
        OPT_FRAME_WINDOWS },
#endif
    HEADER("KEYS AND PROGRESS"),
    { "Open World", "Ashley starts with every key and sigil.", O2(OPT2_OPEN_WORLD) },
    { "Iron Maiden Required", "Great Cathedral needs the Tigertail Sigil from the Iron Maiden. Kali drops its keys.",
        O2(OPT2_IRON_MAIDEN) },
    { "Loot Keys/Sigils", "Random chest items and drops can be keys or sigils too. Vanilla ones stay put.",
        OPT_LOOT_KEYS },
#if !ZENITH_EDITION /* the real Zenith provides these */
    HEADER("ZENITH"),
    { "Perfect Analyze", "Analyze never misses.", OPT_ZENITH_ANALYZE },
    { "Kind Manabreaker", "A Manabreaker gem no longer blocks your own spells.", OPT_ZENITH_MANABREAKER },
    { "Boss Grimoires", "Five bosses drop a Warlock Grimoire.", OPT_ZENITH_BOSS_GRIMOIRES },
    { "Quiet Foes", "Gremlins and Crimson Blades stop casting Herakles and Prostasia.", OPT_ZENITH_QUIET },
    { "Free Teleport", "Teleporting costs no MP.", OPT_ZENITH_TELEPORT },
    { "Kill Drops", "Killing the same enemy again and again raises its equipment drop rate.",
        OPT_ZENITH_DROPS },
#endif
};

typedef struct {
    const char* name;
    const char* help;
    opts_t options;
} preset_t;

static const preset_t presets[] = {
    { "Standard", "Everything randomized, with balanced amounts, stats and gear.",
        PRESET_STANDARD | O2(PRESET2_STANDARD) },
    { "Chaos", "Everything randomized, nothing balanced, plus keys and sigils in loot.",
        PRESET_CHAOS | O2(PRESET2_CHAOS) },
    { "Vanilla", "No randomization.", 0 },
};

#define PRESET_CUSTOM ((int)COUNT(presets))

/* --- screens -------------------------------------------------------------- */

enum { SCREEN_MAIN, SCREEN_SEED, SCREEN_OPTIONS, SCREEN_SAVE };

enum { MAIN_SEED, MAIN_RANDOMIZE, MAIN_PRESET, MAIN_OPTIONS, MAIN_LOAD, MAIN_VANILLA, MAIN_COUNT };

static const char* const mainNames[MAIN_COUNT] = {
    "Enter Seed #", "Randomize", "Select Preset", "Select Options", "Load Seed MemCard", "Launch Vanilla Game",
};

static const char* const mainHelp[MAIN_COUNT] = {
    "Type in a seed to replay or share a run, then start it.",
    "Start a new randomized game with a random seed.",
    NULL, /* the preset's own help */
    "Choose exactly what gets randomized.",
    "Start a game with the seed and options saved on memory card slot 1.",
    "Play the unmodified game.",
};

/* The seed keypad: two rows of five keys. */
#define KEYPAD_COLUMNS 5
static const char keypad[] = "1234567890";
#define KEYPAD_KEYS ((int)sizeof keypad - 1)

/* The options list shows LIST_ROWS entries from LIST_LINE + 1, with "more.."
 * on the lines above and below when it scrolls. */
#define LIST_LINE 0
#define LIST_ROWS 20
#define FAST_STEP 10

typedef struct {
    DRAWENV draw[2];
    DISPENV disp[2];
    int buffer;
    int screen;
    int mainRow;
    int optionRow; /* index in options[], never a header */
    int scroll; /* first visible options[] entry */
    int preset;
    opts_t options;
    u32 seed;
    char digits[SEED_DIGITS + 1]; /* seed being typed */
    int digit; /* position being typed */
    int key; /* keypad cursor */
    int saveChoice; /* 1 = yes */
    int done;
    opts_t startOptions;
    const char* status; /* result of the last card action, shown until the next input */
} menu_t;

/* --- text ------------------------------------------------------------------- */

/* Text is drawn as 8x8 sprites straight from the debug font texture FntLoad
 * puts in VRAM (4-bit, 16 glyphs per row from ' ' to '_', CLUT 128 rows
 * below), so each string can have its own colour. */
typedef struct {
    u8 r, g, b;
} color_t;

static const color_t WHITE = { 0x80, 0x80, 0x80 };
static const color_t GREEN = { 0x30, 0xD0, 0x30 };
static const color_t GREY = { 0x60, 0x60, 0x78 };

#define FONT_TPAGE (0x400 | (FONT_Y / 256) << 4 | FONT_X / 64) /* 4-bit, drawing to display allowed */
#define FONT_CLUT ((FONT_Y + 128) << 6 | FONT_X >> 4)

static u32 packet[256];
static int packetLen;

static void flush(void)
{
    if (packetLen > 1) {
        packet[0] = (packetLen - 1) << 24 | 0xFFFFFF;
        DrawPrim(packet);
    }
    packetLen = 0;
}

static void glyph(int x, int y, char c, color_t color)
{
    int i;

    if (c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    }
    if (c <= ' ' || c > '_') {
        return;
    }
    if (packetLen + 3 > (int)COUNT(packet)) {
        flush();
    }
    if (packetLen == 0) {
        packet[1] = 0xE1000000 | FONT_TPAGE;
        packetLen = 2;
    }
    i = c - ' ';
    packet[packetLen++] = 0x74000000 | color.b << 16 | color.g << 8 | color.r; /* 8x8 textured sprite */
    packet[packetLen++] = (y & 0xFFFF) << 16 | (x & 0xFFFF);
    packet[packetLen++] = FONT_CLUT << 16 | (i / 16 * 8) << 8 | (i % 16 * 8);
}

/* Text at a column and line; '\n' starts the next line at the same column. */
static void text(int col, int line, const char* s, color_t color)
{
    int c = col;

    for (; *s; ++s) {
        if (*s == '\n') {
            ++line;
            c = col;
            continue;
        }
        glyph(TEXT_X + c * GLYPH_W, TEXT_Y + line * LINE_H, *s, color);
        ++c;
    }
}

/* Word-wrapped text in a column of the given width (newlines count as
 * spaces). */
static void wrap(int col, int line, int width, const char* s, color_t color)
{
    while (*s) {
        int len = 0;
        int cut = 0;
        int i;
        while (*s == ' ' || *s == '\n') {
            ++s;
        }
        for (; s[len] && len < width; ++len) {
            if (s[len] == ' ' || s[len] == '\n') {
                cut = len;
            }
        }
        if (!s[len] || s[len] == ' ' || s[len] == '\n' || cut == 0) {
            cut = len;
        }
        for (i = 0; i < cut; ++i) {
            glyph(TEXT_X + (col + i) * GLYPH_W, TEXT_Y + line * LINE_H, s[i], color);
        }
        s += cut;
        ++line;
    }
}

static void formatSeed(char* out, u32 seed)
{
    int i;

    for (i = SEED_DIGITS - 1; i >= 0; --i) {
        out[i] = '0' + seed % 10;
        seed /= 10;
    }
    out[SEED_DIGITS] = 0;
}

/* --- options state ---------------------------------------------------------- */

static int fieldShift(opts_t mask)
{
    int shift = 0;

    while (!(mask & 1)) {
        mask >>= 1;
        ++shift;
    }
    return shift;
}

static int optionValue(const menu_t* m, const option_t* o)
{
    return (m->options & o->bit) >> fieldShift(o->bit);
}

static void changeOption(menu_t* m, const option_t* o, int dir)
{
    int count = 0;
    int value;

    if (o->values == NULL) {
        m->options ^= o->bit;
        return;
    }
    while (o->values[count] != NULL) {
        ++count;
    }
    value = (optionValue(m, o) + dir + count) % count;
    m->options = (m->options & ~o->bit) | ((opts_t)value << fieldShift(o->bit));
}

static void applyPreset(menu_t* m)
{
    if (m->preset < PRESET_CUSTOM) {
        m->options = presets[m->preset].options;
    }
}

/* After toggling, show whichever preset matches the options, or Custom. */
static void matchPreset(menu_t* m)
{
    int i;

    for (i = 0; i < PRESET_CUSTOM; ++i) {
        if (presets[i].options == m->options) {
            m->preset = i;
            return;
        }
    }
    m->preset = PRESET_CUSTOM;
}

static const char* presetName(const menu_t* m)
{
    return m->preset < PRESET_CUSTOM ? presets[m->preset].name : "Custom";
}

/* --- drawing ---------------------------------------------------------------- */

/* Dark blue to black, top to bottom (a Gouraud-shaded quad). */
static void background(void)
{
    static u32 quad[9];

    quad[0] = 8 << 24 | 0xFFFFFF;
    quad[1] = 0x38000000 | 46 << 16 | 18 << 8 | 10;
    quad[2] = 0;
    quad[3] = 46 << 16 | 18 << 8 | 10;
    quad[4] = SCREEN_W;
    quad[5] = 3 << 16;
    quad[6] = SCREEN_H << 16;
    quad[7] = 3 << 16;
    quad[8] = SCREEN_H << 16 | SCREEN_W;
    DrawPrim(quad);
}

static void hints(int line, const char* const* lines)
{
    for (; *lines; ++lines, ++line) {
        text(RIGHT_COLUMN, line, *lines, WHITE);
    }
}

static void footer(void)
{
    static const char date[] = __DATE__; /* "Mmm dd yyyy" */

    text(0, LAST_LINE, date, WHITE);
    text(RIGHT_COLUMN, LAST_LINE, "VSI Rando v" RANDO_VERSION, WHITE);
}

static void drawMain(const menu_t* m)
{
    static const char* const keys[] = { "X = Confirm", NULL };
    static const char* const presetKeys[] = { "<> = Change", NULL };
    char seed[SEED_DIGITS + 1];
    const char* help = mainHelp[m->mainRow];
    int i;

    text(0, 1, ZENITH_EDITION ? "VAGRANT STORY RANDOMIZER  ZENITH" : "VAGRANT STORY RANDOMIZER", GREEN);
    for (i = 0; i < MAIN_COUNT; ++i) {
        text(2, 3 + i, mainNames[i], WHITE);
    }
    text(0, 3 + m->mainRow, ">", GREEN);
    formatSeed(seed, m->seed);
    text(RIGHT_COLUMN - 2, 3 + MAIN_SEED, "seed =", WHITE);
    text(RIGHT_COLUMN + 5, 3 + MAIN_SEED, seed, WHITE);
    text(RIGHT_COLUMN - 2, 3 + MAIN_PRESET, presetName(m), GREEN);
    if (m->mainRow == MAIN_PRESET) {
        help = m->preset < PRESET_CUSTOM ? presets[m->preset].help : "Your own selection of options.";
    }
    wrap(2, 11, 34, m->status != NULL ? m->status : help, m->status != NULL ? GREEN : GREY);
    hints(19, m->mainRow == MAIN_PRESET ? presetKeys : keys);
    footer();
}

static void drawSeed(const menu_t* m)
{
    static const char* const keys[] = { "Tri   = Random", "X     = Enter", "O     = Back", "Start = Begin", NULL };
    int i;

    text(1, 4, "Enter Seed #", WHITE);
    text(14, 4, m->digits, WHITE);
    text(14 + m->digit, 5, "-", GREEN);
    for (i = 0; i < KEYPAD_KEYS; ++i) {
        int col = 9 + (i % KEYPAD_COLUMNS) * 3;
        int line = 10 + (i / KEYPAD_COLUMNS) * 3;
        char key[2] = { keypad[i], 0 };
        text(col, line, key, i == m->key ? GREEN : WHITE);
        if (i == m->key) {
            text(col, line + 1, "-", GREEN);
        }
    }
    hints(17, keys);
    footer();
}

static void drawOptions(const menu_t* m)
{
    static const char* const keys[] = { "L1 = Fast Up", "R1 = Fast Down", "X  = Toggle", "O  = Done", NULL };
    const option_t* sel = &options[m->optionRow];
    int line = LIST_LINE + 1;
    int i;

    if (m->scroll > 0) {
        text(1, LIST_LINE, "more..", GREY);
    }
    for (i = m->scroll; i < (int)COUNT(options) && i < m->scroll + LIST_ROWS; ++i, ++line) {
        const option_t* o = &options[i];
        if (o->bit == 0) {
            text(1, line, o->name, GREEN);
            continue;
        }
        if (m->options & o->bit) {
            text(1, line, "x", WHITE);
        }
        text(2, line, o->name, WHITE);
        if (i == m->optionRow) {
            text(0, line, ">", GREEN);
        }
    }
    if (i < (int)COUNT(options)) {
        text(1, line, "more..", GREY);
    }

    text(RIGHT_COLUMN, 0, presetName(m), GREEN);
    line = 2;
    if (sel->values != NULL) {
        text(RIGHT_COLUMN, line, "<", GREEN);
        text(RIGHT_COLUMN + 1, line, sel->values[optionValue(m, sel)], GREEN);
        text(RIGHT_COLUMN + RIGHT_WIDTH, line, ">", GREEN);
        line += 2;
    }
    wrap(RIGHT_COLUMN, line, RIGHT_WIDTH, sel->help, WHITE);
    hints(17, keys);
    footer();
}

static void drawSave(const menu_t* m)
{
    static const char* const keys[] = { "X = Confirm", "O = Back", NULL };

    wrap(2, 5, 34, "Save this seed and these options to memory card slot 1 before starting?", WHITE);
    text(8, 10, "YES", WHITE);
    text(20, 10, "NO", WHITE);
    text(m->saveChoice ? 6 : 18, 10, ">", GREEN);
    if (m->status != NULL) {
        wrap(2, 13, 34, m->status, GREEN);
    }
    hints(19, keys);
    footer();
}

static void draw(menu_t* m)
{
    m->buffer ^= 1;
    PutDrawEnv(&m->draw[m->buffer]);
    background();
    switch (m->screen) {
    case SCREEN_MAIN:
        drawMain(m);
        break;
    case SCREEN_SEED:
        drawSeed(m);
        break;
    case SCREEN_OPTIONS:
        drawOptions(m);
        break;
    case SCREEN_SAVE:
        drawSave(m);
        break;
    }
    flush();
    DrawSync(0);
    VSync(0);
    PutDispEnv(&m->disp[m->buffer]);
}

/* --- input ------------------------------------------------------------------ */

static u32 randomSeed(void)
{
    rng_t r;
    rng_init(&r, VSync(-1), vs_main_buttonsState);
    return rng_next(&r) % (SEED_MAX + 1);
}

/* Start with these options, asking first whether to save a randomized setup. */
static void start(menu_t* m, opts_t opts)
{
    m->startOptions = opts;
    if (opts == 0) {
        m->done = 1;
        return;
    }
    m->screen = SCREEN_SAVE;
    m->saveChoice = 1;
    m->status = NULL;
}

/* Card I/O blocks for a moment; say so on screen first. */
static void showBusy(menu_t* m)
{
    m->status = "Accessing memory card...";
    draw(m);
    draw(m);
}

static void loadAndStart(menu_t* m)
{
    cardSettings_t s;
    int result;

    showBusy(m);
    result = card_load(&s);
    if (result == CARD_OK) {
        m->seed = s.seed;
        m->startOptions = s.options | O2(s.options2);
        m->done = 1;
        return;
    }
    m->status = result == CARD_NO_FILE  ? "No saved seed on the memory card in slot 1."
              : result == CARD_BAD_FILE ? "The saved seed is damaged."
                                        : "Could not read the memory card in slot 1.";
}

static void updateMain(menu_t* m, u32 pressed)
{
    int dir = (pressed & PAD_RIGHT) ? 1 : (pressed & PAD_LEFT) ? -1 : 0;

    if (pressed & PAD_UP) {
        m->mainRow = (m->mainRow + MAIN_COUNT - 1) % MAIN_COUNT;
    } else if (pressed & PAD_DOWN) {
        m->mainRow = (m->mainRow + 1) % MAIN_COUNT;
    }
    if (m->mainRow == MAIN_PRESET && (dir || (pressed & PAD_CROSS))) {
        m->preset = (m->preset + PRESET_CUSTOM + (dir ? dir : 1)) % PRESET_CUSTOM;
        applyPreset(m);
        return;
    }
    if (!(pressed & PAD_CROSS)) {
        return;
    }
    switch (m->mainRow) {
    case MAIN_SEED:
        formatSeed(m->digits, m->seed);
        m->digit = 0;
        m->screen = SCREEN_SEED;
        break;
    case MAIN_RANDOMIZE:
        m->seed = randomSeed();
        start(m, m->options);
        break;
    case MAIN_OPTIONS:
        m->screen = SCREEN_OPTIONS;
        break;
    case MAIN_LOAD:
        loadAndStart(m);
        break;
    case MAIN_VANILLA:
        start(m, 0);
        break;
    }
}

static u32 typedSeed(const menu_t* m)
{
    u32 seed = 0;
    int i;

    for (i = 0; i < SEED_DIGITS; ++i) {
        seed = seed * 10 + (m->digits[i] - '0');
    }
    return seed;
}

static void updateSeed(menu_t* m, u32 pressed)
{
    if (pressed & PAD_LEFT) {
        m->key = (m->key + KEYPAD_KEYS - 1) % KEYPAD_KEYS;
    } else if (pressed & PAD_RIGHT) {
        m->key = (m->key + 1) % KEYPAD_KEYS;
    } else if (pressed & (PAD_UP | PAD_DOWN)) {
        m->key = (m->key + KEYPAD_COLUMNS) % KEYPAD_KEYS;
    } else if (pressed & PAD_TRIANGLE) {
        formatSeed(m->digits, randomSeed());
        m->digit = 0;
    } else if (pressed & PAD_CROSS) {
        m->digits[m->digit] = keypad[m->key];
        if (++m->digit == SEED_DIGITS) {
            m->digit = 0;
            m->seed = typedSeed(m);
            start(m, m->options);
        }
    } else if (pressed & PAD_CIRCLE) {
        if (m->digit > 0) {
            --m->digit;
        } else {
            m->screen = SCREEN_MAIN;
        }
    } else if (pressed & PAD_START) {
        m->seed = typedSeed(m);
        start(m, m->options);
    }
}

/* Moves the options cursor over steps options (headers are skipped): single
 * steps wrap around, fast steps stop at the ends. Then keeps it in view, with
 * a section's header shown above its first option. */
static void moveOption(menu_t* m, int dir, int steps)
{
    int n = COUNT(options);
    int row = m->optionRow;

    while (steps-- > 0) {
        int next = row;
        do {
            next += dir;
        } while (next >= 0 && next < n && options[next].bit == 0);
        if (next < 0 || next >= n) {
            if (row != m->optionRow) {
                break;
            }
            next = dir > 0 ? 1 : n - 1; /* wrap: first option after the first header, or the last one */
        }
        row = next;
    }
    m->optionRow = row;
    if (row - 1 < m->scroll) {
        m->scroll = row - 1;
    } else if (row >= m->scroll + LIST_ROWS) {
        m->scroll = row - LIST_ROWS + 1;
    }
}

static void updateOptions(menu_t* m, u32 pressed)
{
    const option_t* o = &options[m->optionRow];

    if (pressed & PAD_UP) {
        moveOption(m, -1, 1);
    } else if (pressed & PAD_DOWN) {
        moveOption(m, 1, 1);
    } else if (pressed & PAD_L1) {
        moveOption(m, -1, FAST_STEP);
    } else if (pressed & PAD_R1) {
        moveOption(m, 1, FAST_STEP);
    } else if (pressed & PAD_CROSS) {
        changeOption(m, o, 1);
        matchPreset(m);
    } else if (pressed & (PAD_LEFT | PAD_RIGHT)) {
        if (o->values != NULL) {
            changeOption(m, o, (pressed & PAD_RIGHT) ? 1 : -1);
            matchPreset(m);
        }
    } else if (pressed & (PAD_CIRCLE | PAD_START)) {
        m->screen = SCREEN_MAIN;
    }
}

static void updateSave(menu_t* m, u32 pressed)
{
    cardSettings_t s;

    if (pressed & (PAD_LEFT | PAD_RIGHT)) {
        m->saveChoice ^= 1;
        m->status = NULL;
    } else if (pressed & PAD_CIRCLE) {
        m->screen = SCREEN_MAIN;
        m->status = NULL;
    } else if (pressed & (PAD_CROSS | PAD_START)) {
        if (!m->saveChoice) {
            m->done = 1;
            return;
        }
        showBusy(m);
        s.seed = m->seed;
        s.options = (u32)m->startOptions;
        s.options2 = (u32)(m->startOptions >> 32);
        if (card_save(&s) == CARD_OK) {
            m->done = 1;
            return;
        }
        m->status = "Could not save to slot 1. Pick NO to start without saving.";
    }
}

static void update(menu_t* m)
{
    u32 pressed = vs_main_buttonRepeat;

    if ((pressed & 0xFFFF) && m->screen != SCREEN_SAVE) {
        m->status = NULL;
    }
    switch (m->screen) {
    case SCREEN_MAIN:
        updateMain(m, pressed);
        break;
    case SCREEN_SEED:
        updateSeed(m, pressed);
        break;
    case SCREEN_OPTIONS:
        updateOptions(m, pressed);
        break;
    case SCREEN_SAVE:
        updateSave(m, pressed);
        break;
    }
}

/* --- entry ------------------------------------------------------------------ */

static void initDisplay(menu_t* m)
{
    int i;

    SetDefDrawEnv(&m->draw[0], 0, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&m->draw[1], 0, 256, SCREEN_W, SCREEN_H);
    SetDefDispEnv(&m->disp[0], 0, 256, SCREEN_W, SCREEN_H);
    SetDefDispEnv(&m->disp[1], 0, 0, SCREEN_W, SCREEN_H);
    for (i = 0; i < 2; ++i) {
        m->disp[i].screen.y = 8;
        m->disp[i].screen.h = 224;
    }
    DrawSync(0);
    FntLoad(FONT_X, FONT_Y);
}

/* Entry point: tools/build.py and boot.c rely on it being the first code in
 * the binary. */
__attribute__((section(".text.entry"))) void rando_menuMain(void)
{
    static menu_t m;
    static const RECT screens = { 0, 0, SCREEN_W, 512 };

    vs_main_bzero(&m, sizeof m);
    m.seed = randomSeed();
    m.optionRow = 1;
    m.mainRow = MAIN_RANDOMIZE;
    applyPreset(&m);
    initDisplay(&m);

    do {
        draw(&m);
        vs_main_processPadState();
        update(&m);
    } while (!m.done);

    rando_pending.seed = m.seed;
    rando_pending.options = (u32)m.startOptions;
    rando_pending.options2 = (u32)(m.startOptions >> 32);

    /* Leave a blank screen while TITLE.PRG loads. */
    ClearImage((RECT*)&screens, 0, 0, 0);
    DrawSync(0);
}
