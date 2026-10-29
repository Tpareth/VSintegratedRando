/* Saving and loading randomizer settings on memory card slot 1. */
#pragma once
#include "vs.h"

typedef struct {
    u32 seed;
    u32 options;
    u32 options2;
} cardSettings_t;

enum {
    CARD_OK,
    CARD_NO_FILE, /* card readable, no settings saved on it */
    CARD_BAD_FILE, /* settings file present but damaged */
    CARD_ERROR, /* no card, unformatted, full, or I/O failure */
};

int card_save(const cardSettings_t* s);
int card_load(cardSettings_t* s);
