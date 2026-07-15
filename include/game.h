#ifndef GAME_H
#define GAME_H

#include "common.h"

/** @brief Game-code per-frame VSync handler (dispatched for RENDER_GAME). */
void vsyncGameHandler(void);

/** @brief Main game state-machine loop, driven by g_vsyncRate. */
void gameStateLoop(void);


/* Ability name/description lookups (game.c). */
u8 *getAbilityName(s32 abilityId);
u8 *getAbilityDesc(s32 abilityId);
s32 getAbilityEntryDesc(s32 entryId);

#endif /* GAME_H */
