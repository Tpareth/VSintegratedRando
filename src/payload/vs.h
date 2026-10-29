/* Vagrant Story (USA) game interface: types and functions we use.
 * Struct layouts come from the Rood Reverse decompilation (CC0). Addresses
 * are bound in game.ld. */
#pragma once

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define STATIC_ASSERT(c, m) typedef char static_assert_##m[(c) ? 1 : -1]

/* --- items -------------------------------------------------------------- */

typedef struct {
    u8 id;
    s8 subId;
    s8 wepId;
    s8 category;
    u16 maxDp;
    u16 maxPp;
    u16 currentDp;
    u16 currentPp;
    s8 strength;
    s8 intelligence;
    s8 agility;
    s8 cost;
    u8 damageCostType; /* damageType:2 costType:3 */
    s8 unk11;
    u16 unk12;
    u8 range[4];
    s8 classes[8];
    s8 affinities[8];
    u16 material;
    s8 assembledWeaponIndex;
    s8 index;
} vs_blade;

typedef struct {
    u16 id;
    s8 subId;
    u8 category;
    s8 gemSlots;
    s8 strength;
    s8 intelligence;
    s8 agility;
    s8 types[4];
    u16 assembledWeaponIndex;
    u16 index;
} vs_grip;

typedef struct {
    u16 id;
    s8 subId;
    s8 unk3;
    s8 gemEffects;
    s8 strength;
    s8 intelligence;
    s8 agility;
    s8 classes[8];
    s8 affinities[8];
    u16 setItemIndex;
    s16 index;
} vs_gem;

typedef struct {
    u8 id;
    u8 subId;
    s8 wepId;
    s8 category;
    u16 maxDp;
    u16 maxPp;
    u16 currentDp;
    u16 currentPp;
    s8 gemSlots;
    s8 strength;
    s8 intelligence;
    s8 agility;
    s8 types[4];
    s8 classes[8];
    s8 affinities[8];
    u16 material;
    u8 limb;
    u8 index;
} vs_armor;

STATIC_ASSERT(sizeof(vs_blade) == 0x2C, blade);
STATIC_ASSERT(sizeof(vs_grip) == 0x10, grip);
STATIC_ASSERT(sizeof(vs_gem) == 0x1C, gem);
STATIC_ASSERT(sizeof(vs_armor) == 0x28, armor);

/* --- inventory (vs_main_inventory_t) ---------------------------------------- */

typedef struct {
    u8 index;
    u8 blade; /* 1-based slot in blades[], 0 = none */
    u8 grip; /* 1-based slot in grips[] */
    u8 isEquipped;
    u8 gems[4];
    char name[24];
} vs_inventoryWeapon;

typedef struct {
    u8 index;
    u8 isEquipped;
    s16 unk2;
    vs_armor base;
    u8 gems[4];
} vs_inventoryShield;

typedef struct {
    u16 id;
    u8 count;
    u8 index;
} vs_inventoryMisc;

typedef struct {
    vs_inventoryWeapon weapons[8];
    vs_inventoryShield shields[8];
    vs_blade blades[16];
    vs_grip grips[16];
    vs_armor armor[16];
    vs_gem gems[48];
    vs_inventoryMisc misc[64];
} vs_inventory;

STATIC_ASSERT(sizeof(vs_inventory) == 0xF00, inventory);

/* --- MPD treasure section (vs_battle_loot) --------------------------------- */

/* difficultyFlags: an entry is present when non-zero (3 = both play-throughs). */
typedef struct {
    u8 difficultyFlags, unk1, unk2, unk3;
    vs_blade blade;
    vs_grip grip;
    vs_gem gems[3];
    char name[24];
} vs_lootWeapon;

typedef struct {
    u8 difficultyFlags, unk1, unk2, unk3;
    vs_blade blade;
} vs_lootBlade;

typedef struct {
    u8 difficultyFlags, unk1, unk2, unk3;
    vs_grip grip;
} vs_lootGrip;

typedef struct {
    u8 difficultyFlags, unk1, unk2, unk3;
    vs_armor shield;
    vs_gem gems[3];
} vs_lootShield;

typedef struct {
    u8 difficultyFlags, unk1, unk2, unk3;
    vs_armor armor;
} vs_lootArmor;

typedef struct {
    u8 difficultyFlags, unk1, unk2, unk3;
    vs_gem gem;
} vs_lootGem;

typedef struct {
    u16 id;
    u8 difficultyFlags;
    u8 count;
} vs_lootMisc;

typedef struct {
    vs_lootWeapon weapon;
    vs_lootBlade blade;
    vs_lootGrip grip;
    vs_lootShield shield;
    vs_lootArmor armor[2];
    vs_lootArmor accessory;
    vs_lootGem gem;
    vs_lootMisc misc[3];
} vs_loot;

STATIC_ASSERT(sizeof(vs_loot) == 0x220, loot);

/* --- MPD file --------------------------------------------------------------- */

typedef struct {
    u32 offset;
    u32 length;
} vs_section;

typedef struct {
    vs_section room, cleared, script, door, enemy, treasure;
} vs_mpdHeader;

typedef struct {
    u8 type, index, hidden, unk3, id, slot, flagTest, statTest;
    u16 flagValue, statValue;
    u8 x, direction, z, height;
    u8 settings[8];
    u8 unk18[2];
    u16 itemId, item2Id, item3Id;
    u8 itemCount, item2Count, item3Count;
    u8 facing, unk24, variant, unk26, sceneTest;
} vs_mpdEnemy;

STATIC_ASSERT(sizeof(vs_mpdEnemy) == 0x28, mpdEnemy);

/* --- ZND file ------------------------------------------------------------- */

/* Enemy template (vs_battle_charInitData). The ZND enemy section is
 * u32 count, count x 8-byte ZUD file entries, count x templates. */
typedef struct {
    u16 subType;
    u8 modelId;
    u8 reach;
    char name[24];
    s16 hp; /* 0x1C */
    s16 mp;
    u8 strength; /* 0x20; read unsigned (vanilla has values up to 250) */
    u8 intelligence;
    u8 agility;
    u8 unk23;
    u8 unk24;
    u8 walkSpeed; /* 0x25 */
    u8 unk26;
    u8 runSpeed; /* 0x27 */
    u8 rest[0x464 - 0x28]; /* weapon, shield, accessory, limbs, mpd id */
} vs_zndEnemy;

STATIC_ASSERT(sizeof(vs_zndEnemy) == 0x464, zndEnemy);

/* --- actions (vs_main_actions: spells, abilities, Break Arts) ------------- */

typedef struct {
    u8 id;
    u8 unk1;
    u8 typeBits; /* cost type and action type */
    u8 cost;
    u8 rest[0x30]; /* range, area, flags, hit parameters, name[24] */
} vs_action_t;

STATIC_ASSERT(sizeof(vs_action_t) == 0x34, action);

extern vs_action_t vs_main_actions[];
extern u16 vs_main_artsPointsRequirements[40]; /* points to learn each Break Art */
extern u16 vs_main_battleAbilitiesPointsRequirements[22]; /* points to learn each ability */

/* --- item id ranges (ITEMNAME.BIN index) ---------------------------------- */

#define ITEM_CONSUMABLE_FIRST 0x143
#define ITEM_CONSUMABLE_LAST 0x164
#define ITEM_GRIMOIRE_FIRST 0x182
#define ITEM_GRIMOIRE_LAST 0x1C8
#define ITEM_KEY_FIRST 0x1CA
#define ITEM_KEY_LAST 0x1D1
#define ITEM_SIGIL_FIRST 0x1D2
#define ITEM_SIGIL_LAST 0x1E9

/* --- game state ----------------------------------------------------------- */

typedef struct {
    s32 zndId;
    u8 unk04[0x30];
    s32 mapId; /* 0x34 */
    u8 unk38[0x24];
    void* mpdData; /* 0x5C */
} vs_zoneContext;

typedef struct {
    u8 zndId;
    u8 mpdId;
} vs_roomRequest;

extern vs_zoneContext _zoneContext;
extern vs_inventory vs_main_inventory;
extern vs_roomRequest D_800F1AB0;

/* --- game functions --------------------------------------------------------- */

/* Pad state bits (vs_main_buttonsState etc.; 1 = held). */
enum {
    PAD_L2 = 1 << 0, PAD_R2 = 1 << 1, PAD_L1 = 1 << 2, PAD_R1 = 1 << 3,
    PAD_TRIANGLE = 1 << 4, PAD_CIRCLE = 1 << 5, PAD_CROSS = 1 << 6, PAD_SQUARE = 1 << 7,
    PAD_SELECT = 1 << 8, PAD_START = 1 << 11,
    PAD_UP = 1 << 12, PAD_RIGHT = 1 << 13, PAD_DOWN = 1 << 14, PAD_LEFT = 1 << 15,
};

extern u32 vs_main_buttonsState;
extern u32 vs_main_buttonsPressed;
/* Newly pressed buttons plus auto-repeat while held. */
extern u32 vs_main_buttonRepeat;

int vs_main_processPadState(void);
u32 vs_main_updatePadState(int port, void* buffer);

typedef struct {
    s32 lba;
    s32 size;
} vs_main_CdFile;

enum { CD_QUEUE_LOADED = 4 };

typedef struct {
    s16 state;
    s16 priority;
    vs_main_CdFile cdFile;
    s32 unkC;
    void* vram;
} vs_main_CdQueueSlot;

vs_main_CdQueueSlot* vs_main_allocateCdQueueSlot(vs_main_CdFile* file);
void vs_main_cdEnqueuePriority(vs_main_CdQueueSlot* slot, void* dest);
void vs_main_freeCdQueueSlot(vs_main_CdQueueSlot* slot);
int vs_main_gametimeUpdate(int vsync);
void _loadTitlePrg(void);
void FlushCache(void);
void* vs_main_allocHeap(u32 size);
void vs_main_freeHeap(void* block);
void vs_main_bzero(void* dst, u32 size);
void vs_main_memcpy(void* dst, const void* src, u32 size);
int func_80076F24(int slot, vs_zndEnemy* init, int a2, int a3, int a4, int a5); /* create actor */
void _loadZndEnemies(void* data, int len);
/* Draws ASCII text in the game's menu font into the current frame;
 * xy = y << 16 | x, nextPrim NULL for the default layer. */
void vs_battle_renderTextRaw(const char* text, int xy, void* nextPrim);
void vs_mainMenu_exec(int menu); /* MAINMENU.PRG, overlay slot 1 */
void _loadMpdRoomSection(int index, void* room);
