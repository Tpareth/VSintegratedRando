#include "rng.h"

static u32 mix(u32 x)
{
    x ^= x >> 16;
    x *= 0x7FEB352D;
    x ^= x >> 15;
    x *= 0x846CA68B;
    x ^= x >> 16;
    return x;
}

void rng_init(rng_t* r, u32 seed, u32 stream)
{
    r->s = mix(seed ^ mix(stream + 0x9E3779B9));
    if (r->s == 0) {
        r->s = 1;
    }
}

u32 rng_next(rng_t* r)
{
    /* xorshift32 */
    u32 x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->s = x;
}

int rng_range(rng_t* r, int lo, int hi)
{
    u32 span = (u32)(hi - lo) + 1;
    return lo + (int)(rng_next(r) % span);
}
