/* Randomizer config validity: a magic and a checksum guard the bytes in the
 * save flags, so a vanilla or damaged save plays vanilla. */
#include "rando.h"

static u16 configCheck(const rando_config_t* c)
{
    /* Unsigned throughout: u16 * int would be a signed (overflowing) multiply. */
    u32 x = ((u32)c->magic * 0x27D4EB2Fu) ^ (c->seed * 0x9E3779B1u) ^ (c->options * 0x85EBCA77u)
        ^ (c->options2 * 0xC2B2AE3Du); /* 0 for configs from before options2 */
    return (u16)(x ^ (x >> 16));
}

int rando_enabled(void)
{
    return rando_config.magic == RANDO_CONFIG_MAGIC && rando_config.check == configCheck(&rando_config);
}

void rando_setConfig(u32 seed, u32 options, u32 options2)
{
    rando_config.magic = RANDO_CONFIG_MAGIC;
    rando_config.seed = seed;
    rando_config.options = options;
    rando_config.options2 = options2;
    rando_config.check = configCheck(&rando_config);
}
