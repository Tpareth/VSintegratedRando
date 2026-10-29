/* Stat rolls shared by the battle overlay's randomizers (see roll.h). */
#include "roll.h"

int roll(roller_t* r, const stat_t* s)
{
    const roll_t* x = r->group < 0 ? &s->unbalanced : &s->balanced[r->group];

    if (x->neg && rng_range(&r->rng, 1, 2) == 2) {
        return rng_range(&r->rng, -x->neg, -1);
    }
    return rng_range(&r->rng, x->lo, x->hi);
}

void rollBytes(roller_t* r, u8* p, int count, const stat_t* s)
{
    int i;

    for (i = 0; i < count; ++i) {
        p[i] = roll(r, s);
    }
}
