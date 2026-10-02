#ifndef BATTLE_RESULTS_NUMBER_H
#define BATTLE_RESULTS_NUMBER_H

#include "common.h"
#include "psxsdk/libgpu.h"

void *drawNumber(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color, s32 clut);
void *drawNumberDefault(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color);
s32 drawNumberMenuTint(s32 ot, s32 sprt, s32 x, s32 value, s32 clut);
void drawNumberMenuTintDefault(s32 ot, s32 sprt, s32 x, s32 value);

#endif /* BATTLE_RESULTS_NUMBER_H */
