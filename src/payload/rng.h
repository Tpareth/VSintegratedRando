/* Deterministic RNG. Every randomized decision derives from (seed, stream),
 * where the stream identifies what is being randomized (e.g. a map id), so a
 * room randomizes identically each time it is loaded. */
#pragma once
#include "vs.h"

typedef struct {
    u32 s;
} rng_t;

void rng_init(rng_t* r, u32 seed, u32 stream);
u32 rng_next(rng_t* r);
/* Uniform integer in [lo, hi] (inclusive). */
int rng_range(rng_t* r, int lo, int hi);
