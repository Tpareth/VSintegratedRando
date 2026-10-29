/* Randomizer settings file on memory card slot 1, through the BIOS file
 * functions linked into SLUS_010.40 (blocking calls; the game starts the card
 * driver at boot). The file is a normal one-block save with a title and icon,
 * so it shows up in the console's memory card manager. The game's own save
 * screen only looks at its "BASLUS-01040VAGn" files and just counts this
 * one as used space. */
#include "card.h"

#define FILENAME "bu00:BASLUS-01040RNDO"

/* BIOS open() flags. */
#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_CREAT 0x0200
#define BLOCKS(n) ((n) << 16)

int open(const char* name, int flags);
int read(int fd, void* buf, int n);
int write(int fd, const void* buf, int n);
int close(int fd);
int erase(const char* name);

#define SETTINGS_MAGIC 0x444E5253 /* "SRND" */
#define SETTINGS_VERSION 3 /* 1 had 16-bit options, 2 no options2 */

/* BIOS save file: header frame, one icon frame, then our data. Card I/O is in
 * 128-byte sectors. */
typedef struct {
    char magic[2]; /* "SC" */
    u8 iconFlag; /* 0x11: one static icon frame */
    u8 blocks;
    u8 title[64]; /* Shift-JIS */
    u8 pad[28];
    u16 clut[16];
    u8 icon[128]; /* 16x16, 4 bits per pixel */
    struct {
        u32 magic;
        u32 version;
        u32 seed;
        u32 options; /* version 1: u16 options, u16 check */
        u16 check;
        u16 pad0;
        u32 options2; /* version 3 */
        u8 pad[104];
    } data;
} settingsFile_t;

STATIC_ASSERT(sizeof(settingsFile_t) == 0x180, settingsFile);

static settingsFile_t file;

static const char title[] = "VS RANDOMIZER SETTINGS";

/* A key, in palette indices (. = 0 transparent, other digits = colours). */
static const char icon[16][17] = {
    "................",
    "....111111......",
    "...12222221.....",
    "..1223333221....",
    "..123....321....",
    "..123....321....",
    "..1223333221....",
    "...12222221.....",
    "....112211......",
    ".....1221.......",
    ".....12211111...",
    ".....12222221...",
    ".....12211111...",
    ".....122111.....",
    ".....12221......",
    "......111.......",
};

static const u16 iconClut[4] = { 0x0000, 0x8C63, 0x0BDF, 0x0110 }; /* clear, dark, gold, brown */

static u16 checksum(u32 seed, u32 options, u32 options2)
{
    u32 x = SETTINGS_MAGIC ^ (seed * 0x9E3779B1) ^ (options * 0x85EBCA77) ^ (options2 * 0xC2B2AE3D); /* 0 before v3 */
    return (u16)(x ^ (x >> 16));
}

/* Shift-JIS full-width form of an ASCII title (letters, digits, space). */
static void titleToSjis(u8* out, const char* s)
{
    int i = 0;

    for (; *s && i < 64; ++s, i += 2) {
        u16 c = 0x8140; /* full-width space */
        if (*s >= 'A' && *s <= 'Z') {
            c = 0x8260 + (*s - 'A');
        } else if (*s >= '0' && *s <= '9') {
            c = 0x824F + (*s - '0');
        }
        out[i] = c >> 8;
        out[i + 1] = c & 0xFF;
    }
}

static void buildFile(const cardSettings_t* s)
{
    int x, y;

    vs_main_bzero(&file, sizeof file);
    file.magic[0] = 'S';
    file.magic[1] = 'C';
    file.iconFlag = 0x11;
    file.blocks = 1;
    titleToSjis(file.title, title);
    vs_main_memcpy(file.clut, iconClut, sizeof iconClut);
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            char c = icon[y][x];
            u8 v = c == '.' ? 0 : c - '0';
            file.icon[y * 8 + x / 2] |= x & 1 ? v << 4 : v;
        }
    }
    file.data.magic = SETTINGS_MAGIC;
    file.data.version = SETTINGS_VERSION;
    file.data.seed = s->seed;
    file.data.options = s->options;
    file.data.options2 = s->options2;
    file.data.check = checksum(s->seed, s->options, s->options2);
}

int card_save(const cardSettings_t* s)
{
    int fd;
    int ok;

    buildFile(s);
    erase(FILENAME);
    fd = open(FILENAME, O_CREAT | BLOCKS(1));
    if (fd < 0) {
        return CARD_ERROR;
    }
    close(fd);
    fd = open(FILENAME, O_WRONLY);
    if (fd < 0) {
        return CARD_ERROR;
    }
    ok = write(fd, &file, sizeof file) == sizeof file;
    close(fd);
    return ok ? CARD_OK : CARD_ERROR;
}

int card_load(cardSettings_t* s)
{
    int fd = open(FILENAME, O_RDONLY);
    int ok;

    if (fd < 0) {
        return CARD_NO_FILE;
    }
    ok = read(fd, &file, sizeof file) == sizeof file;
    close(fd);
    if (!ok) {
        return CARD_ERROR;
    }
    if (file.data.magic != SETTINGS_MAGIC) {
        return CARD_BAD_FILE;
    }
    if (file.data.version == 1) {
        u16 options = file.data.options & 0xFFFF;
        u16 check = file.data.options >> 16;
        if (check != checksum(file.data.seed, options, 0)) {
            return CARD_BAD_FILE;
        }
        file.data.options = options;
        file.data.options2 = 0;
    } else if (file.data.version < 2 || file.data.version > SETTINGS_VERSION
        || file.data.check != checksum(file.data.seed, file.data.options,
            file.data.version == 2 ? 0 : file.data.options2)) {
        return CARD_BAD_FILE;
    }
    s->seed = file.data.seed;
    s->options = file.data.options;
    s->options2 = file.data.version == 2 ? 0 : file.data.options2;
    return CARD_OK;
}
