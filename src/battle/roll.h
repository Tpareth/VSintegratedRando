/* Stat rolls shared by the battle overlay's randomizers. */
#pragma once
#include "rando.h"

/* Uniform lo..hi; with neg, a coin flip first picks -neg..-1 instead. Values
 * are stored as bytes, so 0..255 also covers the negative half. */
typedef struct {
    s16 lo, hi;
    u8 neg;
} roll_t;

#define R(lo, hi) { lo, hi, 0 }
#define SPLIT(pos, neg) { 0, pos, neg }
#define ANY R(0, 255)

typedef struct {
    roll_t unbalanced;
    roll_t balanced[5]; /* by story group */
} stat_t;

typedef struct {
    rng_t rng;
    int group; /* story group, or -1 for the unbalanced ranges */
} roller_t;

int roll(roller_t* r, const stat_t* s);
void rando_rollCostType(rng_t* rng, vs_blade* blade);
void rollBytes(roller_t* r, u8* p, int count, const stat_t* s);

static inline u16 get16(const u8* p)
{
    return p[0] | p[1] << 8;
}

static inline void put16(u8* p, u32 v)
{
    p[0] = v;
    p[1] = v >> 8;
}

