/* Psy-Q libgpu/libetc functions linked into SLUS_010.40 (addresses in game.ld). */
#pragma once
#include "vs.h"

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    u32 dr_env[16];
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

STATIC_ASSERT(sizeof(DRAWENV) == 0x5C, drawenv);
STATIC_ASSERT(sizeof(DISPENV) == 0x14, dispenv);

/* Flat-shaded rectangle (GP0 0x60), terminated so it can go to DrawPrim. */
typedef struct {
    u32 tag;
    u8 r, g, b, code;
    s16 x, y;
    s16 w, h;
} TILE;

int VSync(int mode);
int DrawSync(int mode);
void DrawPrim(void* prim);
int ClearImage(RECT* rect, int r, int g, int b);
int LoadImage(RECT* rect, void* data);
int StoreImage(RECT* rect, void* data);
DRAWENV* PutDrawEnv(DRAWENV* env);
DISPENV* PutDispEnv(DISPENV* env);
DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h);
DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h);

/* Debug font: FntLoad uploads the font (and its CLUT at ty+128) to VRAM. */
void FntLoad(int tx, int ty);
int FntOpen(int x, int y, int w, int h, int isbg, int n);
int FntPrint(int id, const char* fmt, ...);
void* FntFlush(int id);
