#pragma once
#include "rng.h"
#include "vs.h"

#define RANDO_CONFIG_MAGIC 0x5652 /* "RV" */

/* Zenith edition (`make zenith`): built on top of Vagrant Story Zenith 1.94,
 * which has its own versions of the Zenith options and a rearranged action
 * table (vs_main_actions moved to 0x8004B9E0 with another record layout), so
 * the Zenith options and everything that edits action records are left out.
 * The Break Art and battle ability point tables are where vanilla has them. */
#ifdef RANDO_ZENITH_EDITION
#define ZENITH_EDITION 1
#else
#define ZENITH_EDITION 0
#endif

enum {
    OPT_CHESTS = 1 << 0, /* randomize chest consumables and grimoires */
    OPT_BALANCE_QUANTITIES = 1 << 1, /* vanilla-like consumable counts, chest gear by area */
    OPT_ENEMY_STATS = 1 << 2, /* enemy HP/MP/STR/INT/AGL and speeds */
    OPT_BALANCE_ENEMIES = 1 << 3, /* enemy stat caps scale with story progress */
    OPT_ENEMY_DROPS = 1 << 4, /* chance-based enemy drop item and rate */
    OPT_ASHLEY_STATS = 1 << 5, /* Ashley's HP/MP/STR/INT/AGL and speeds */
    OPT_ASHLEY_WEAPON = 1 << 6, /* starting blade and grip */
    OPT_ASHLEY_ARMOR = 1 << 7, /* starting armor and accessory */
    OPT_ASHLEY_ITEMS = 1 << 8, /* starting misc items */
    OPT_BALANCE_ASHLEY = 1 << 9, /* early-game stat ranges and Bronze/Iron gear */
    OPT_LOOT_KEYS = 1 << 10, /* random chest items and drops can be keys/sigils */
    OPT_CHEST_GEAR = 1 << 11, /* chest weapons, blades, grips, shields, armor, gems */
    /* bits 12-15: Ashley build (OPT_BUILD_MASK) */
    OPT_BREAK_ARTS = 1 << 16, /* Break Art use costs and learning points */
    OPT_BALANCE_BREAK_ARTS = 1 << 17, /* ... kept within the vanilla ranges */
    OPT_CHAIN_LEARNING = 1 << 18, /* points to learn battle abilities */
    OPT_RISK_COSTS = 1 << 19, /* risk cost of chain and defense abilities */
    OPT_ULTIMATE_SPELLS = 1 << 20, /* Lv.4 elemental spells become the ultimates */
    OPT_FRAME_WINDOWS = 1 << 21, /* chain/defense input window length */
    OPT_ZENITH_ANALYZE = 1 << 22, /* Zenith: Analyze never misses */
    OPT_ZENITH_MANABREAKER = 1 << 23, /* Zenith: Manabreaker lets friendly spells through */
    OPT_ZENITH_BOSS_GRIMOIRES = 1 << 24, /* Zenith: five bosses drop Warlock Grimoires */
    OPT_ZENITH_QUIET = 1 << 25, /* Zenith: Gremlins/Crimson Blades lose their buff spells */
    OPT_ZENITH_TELEPORT = 1 << 26, /* Zenith: teleporting costs no MP */
    OPT_ZENITH_DROPS = 1 << 27, /* Zenith: repeat kills raise equipment drop rates */
};

/* Second option word (rando_config.options2). */
enum {
    OPT2_ENEMY_GEAR_BASE = 1 << 0, /* enemy equipment: ids, STR/INT/AGL, DP/PP, damage, material, drop rates */
    OPT2_ENEMY_GEAR_CLASS = 1 << 1, /* enemy equipment class values */
    OPT2_ENEMY_GEAR_AFFINITY = 1 << 2, /* enemy equipment affinity values */
    OPT2_ENEMY_GEAR_BEP = 1 << 3, /* enemy equipment blunt/edged/piercing values */
    OPT2_ENEMY_APPENDAGES = 1 << 4, /* enemy body parts: HP, defense, affinities, BEP */
    OPT2_CHEST_GEAR_BASE = 1 << 5, /* chest equipment DP/PP and gem STR/INT/AGL */
    OPT2_CHEST_GEAR_CLASS = 1 << 6, /* chest equipment class values */
    OPT2_CHEST_GEAR_AFFINITY = 1 << 7, /* chest equipment affinity values */
    OPT2_CHEST_GEAR_BEP = 1 << 8, /* chest grip/shield/armor blunt/edged/piercing values */
    OPT2_BALANCE_ITEM_STATS = 1 << 9, /* chest equipment stat ranges by point in the story */
    OPT2_ASHLEY_CLASS = 1 << 10, /* Ashley's starting equipment class values */
    OPT2_ASHLEY_AFFINITY = 1 << 11, /* ... affinities, and his body parts' */
    OPT2_ASHLEY_BEP = 1 << 12, /* ... blunt/edged/piercing, and his body parts' */
    /* bits 13-14: balanced Ashley STR/INT/AGL threshold (OPT2_THRESHOLD_MASK) */
    OPT2_BLADE_COST = 1 << 15, /* chest blades: which stat their cost comes out of */
    OPT2_FANDANGO_COST = 1 << 16, /* the same for the starting blade */
    OPT2_ICE_WOLF = 1 << 17, /* City Walls Hellhounds breathe ice */
    OPT2_OPEN_WORLD = 1 << 18, /* start with every key and sigil */
    OPT2_IRON_MAIDEN = 1 << 19, /* Great Cathedral needs the Tigertail Sigil; Kali drops the Gold/Platinum Keys */
};

/* Balanced Ashley STR/INT/AGL range, 100 +- spread (Ashley.cpp's threshold). */
#define OPT2_THRESHOLD_SHIFT 13
#define OPT2_THRESHOLD_MASK (3 << OPT2_THRESHOLD_SHIFT)
enum { THRESHOLD_MEDIUM, THRESHOLD_LOW, THRESHOLD_HIGH }; /* 50-150, 75-125, 25-175 */

/* Ashley build, stored in the top bits of the options (0 = off). A build
 * replaces the Ashley stats, starting weapon and starting items options. */
#define OPT_BUILD_SHIFT 12
#define OPT_BUILD_MASK (0xF << OPT_BUILD_SHIFT)
enum {
    BUILD_OFF,
    BUILD_DAGGER,
    BUILD_GREAT_SWORD,
    BUILD_AXE,
    BUILD_MACE,
    BUILD_GREAT_AXE,
    BUILD_GREAT_MACE,
    BUILD_POLEARM,
    BUILD_STAFF,
    BUILD_CROSSBOW,
    BUILD_RANDOM,
};
#define BUILD_COUNT BUILD_CROSSBOW

/* Preset option sets, shared by the boot menu and the in-game seed display. */
#define OPT_ALL (OPT_CHESTS | OPT_CHEST_GEAR | OPT_ENEMY_STATS | OPT_ENEMY_DROPS | OPT_ASHLEY_STATS | OPT_ASHLEY_WEAPON \
    | OPT_ASHLEY_ARMOR | OPT_ASHLEY_ITEMS | OPT_BREAK_ARTS | OPT_CHAIN_LEARNING | OPT_RISK_COSTS)
#define OPT_BALANCE_ALL (OPT_BALANCE_QUANTITIES | OPT_BALANCE_ENEMIES | OPT_BALANCE_ASHLEY | OPT_BALANCE_BREAK_ARTS)
#define PRESET_STANDARD (OPT_ALL | OPT_BALANCE_ALL)
#define PRESET_CHAOS (OPT_ALL | OPT_LOOT_KEYS | OPT_ULTIMATE_SPELLS | OPT_FRAME_WINDOWS)
#define OPT2_ALL (OPT2_ENEMY_GEAR_BASE | OPT2_ENEMY_GEAR_CLASS | OPT2_ENEMY_GEAR_AFFINITY | OPT2_ENEMY_GEAR_BEP \
    | OPT2_ENEMY_APPENDAGES | OPT2_CHEST_GEAR_BASE | OPT2_CHEST_GEAR_CLASS | OPT2_CHEST_GEAR_AFFINITY | OPT2_CHEST_GEAR_BEP \
    | OPT2_ASHLEY_CLASS | OPT2_ASHLEY_AFFINITY | OPT2_ASHLEY_BEP | OPT2_BLADE_COST | OPT2_FANDANGO_COST | OPT2_ICE_WOLF)
#define PRESET2_STANDARD (OPT2_ALL | OPT2_BALANCE_ITEM_STATS)
#define PRESET2_CHAOS OPT2_ALL

/* RNG stream kinds (top byte of the stream id; the rest identifies the
 * zone/room/entity being randomized). */
enum {
    STREAM_CHESTS = 1 << 24,
    STREAM_ENEMY_STATS = 2 << 24,
    STREAM_ENEMY_DROPS = 3 << 24,
    STREAM_ASHLEY = 4 << 24,
    STREAM_LOOT_KEYS = 5 << 24,
    STREAM_CHEST_GEAR = 6 << 24,
    STREAM_ABILITIES = 7 << 24,
    STREAM_ENEMY_GEAR = 8 << 24,
    STREAM_ITEM_STATS = 9 << 24,
};

/* Seed and options chosen in the New Game menu. Stored in the last bytes of
 * vs_main_stateFlags, which the game writes into every save, so Continue
 * restores them without any save/load hooks. */
typedef struct {
    u32 options2; /* OPT2_*; added later, below the original 12 bytes so older saves still check out */
    u16 magic;
    u16 check;
    u32 seed;
    u32 options;
} rando_config_t;

STATIC_ASSERT(sizeof(rando_config_t) == 16, config); /* flags 0x430-0x43F */

#define RANDO_CONFIG_OFFSET (0x440 - sizeof(rando_config_t))

#ifdef RANDO_HOST
extern rando_config_t rando_config;
#else
extern u8 vs_main_stateFlags[0x440];
#define rando_config (*(rando_config_t*)&vs_main_stateFlags[RANDO_CONFIG_OFFSET])
#endif

/* Non-zero when a valid config is present (i.e. this is a randomized game). */
int rando_enabled(void);
void rando_setConfig(u32 seed, u32 options, u32 options2);

/* Settings chosen in the boot menu, applied when New Game is picked. */
typedef struct {
    u32 seed;
    u32 options;
    u32 options2;
} rando_pending_t;

extern rando_pending_t rando_pending;

/* A file stored in free disc sectors; build.py fills in lba/size. */
#define RANDO_FILE_MAGIC 0x454C4946 /* "FILE" */
typedef struct {
    u32 magic;
    u32 lba;
    u32 size; /* whole sectors */
} rando_file_t;

extern rando_file_t rando_menuFile, rando_overlayFile;
int rando_loadFile(const rando_file_t* file, void* dest);

/* Battle overlay (src/battle): code that only runs while the battle program
 * is up, loaded by rando_onAshleyInit into the free RAM between the end of
 * BATTLE.PRG and overlay slot 1. The resident code calls it through this
 * header, after checking the magic (a vanilla game never loads it). */
#define RANDO_OVERLAY_ADDR 0x800F6000 /* past BATTLE.PRG's end even as extended by the QoL and Zenith mods */
#define RANDO_OVERLAY_END 0x800F9800
#define RANDO_OVERLAY_MAGIC 0x4C56524F /* "ORVL" */
typedef struct {
    u32 magic;
    void (*enemyGear)(void* data, int zndId);
    void (*chestStats)(vs_loot* loot, u32 roomKey);
    void (*ashleyGear)(void); /* starting inventory, once per New Game */
    void (*ashleyBody)(vs_zndEnemy* ashley); /* his template, every battle program start */
    void (*room)(vs_mpdHeader* mpd, u32 roomKey); /* every MPD load, before the game parses it */
} rando_overlay_t;
#define rando_overlay (*(rando_overlay_t*)RANDO_OVERLAY_ADDR)

void rando_enemyGear(void* data, int zndId);
void rando_chestStats(vs_loot* loot, u32 roomKey);
void rando_ashleyGearStats(void);
void rando_ashleyBody(vs_zndEnemy* ashley);
void rando_roomPatches(vs_mpdHeader* mpd, u32 roomKey);
void rando_openWorld(void);
extern int rando_newGameGear; /* set at New Game: roll the starting gear stats once the overlay is in */
int rando_zoneGroup(int zndId);

void rando_chests(vs_loot* loot, u32 roomKey);
void rando_chestGear(vs_loot* loot, u32 roomKey);
void rando_zndEnemies(void* data, int zndId);
void rando_mpdDrops(vs_mpdEnemy* spawns, u32 count, int zndId, u32 roomKey);
void rando_ashleyStats(vs_zndEnemy* ashley);
void rando_abilities(void);
void rando_zenithAnalyze(void);
void rando_zenithBattle(void);
void rando_zenithBossDrops(vs_mpdEnemy* spawns, u32 count, u32 roomKey);
void rando_zenithQuiet(void* data, int zndId);
int rando_dropRoll(int roll, int rate, const u16* item, int kind);
void rando_ashleyGear(void);

/* --- equipment pools (gear.c) --- */

enum { MATERIAL_IRON = 4, MATERIAL_HAGANE = 5, MATERIAL_SILVER = 6, MATERIAL_ANY = 0xFF };
/* Armor category doubles as the body slot. */
enum { ARMOR_HEAD = 2, ARMOR_BODY = 3, ARMOR_LEGS = 4, ARMOR_ARMS = 5, ARMOR_ACCESSORY = 7 };

int gear_gripCategoryFor(int bladeCategory);
const vs_blade* gear_pickBlade(rng_t* rng, int maxMaterial);
const vs_grip* gear_pickGrip(rng_t* rng, int category);
const vs_armor* gear_pickShield(rng_t* rng, int maxMaterial);
const vs_armor* gear_pickArmor(rng_t* rng, int category, int maxMaterial);
const vs_gem* gear_pickGem(rng_t* rng);
const vs_blade* gear_findBlade(int id);
const vs_grip* gear_findGrip(int id);

int rando_build(void);
void rando_buildStats(vs_zndEnemy* ashley, int build);
void rando_buildGear(vs_inventory* inv, int build);

/* Balance group (story stage, 0-4) of a randomized zone, or -1. */
int rando_zoneGroup(int zndId);

int item_isConsumable(int id);
int item_isGrimoire(int id);
int item_isProgression(int id);
u16 item_randomConsumable(rng_t* rng);
#define ITEM_CONSUMABLE_COUNT (ITEM_CONSUMABLE_LAST - ITEM_CONSUMABLE_FIRST + 1)
extern const int item_grimoireCount;
u16 item_grimoireAt(int i);
u16 item_randomGrimoire(rng_t* rng);
u16 item_randomMisc(rng_t* rng);
void rando_beforeTitle(void);
void rando_onMainMenu(int menu);
int rando_onNewGame(void);
