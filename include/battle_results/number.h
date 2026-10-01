#ifndef BATTLE_RESULTS_NUMBER_H
#define BATTLE_RESULTS_NUMBER_H

#include "common.h"
#include "psxsdk/libgpu.h"

void *func_800330F4(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color, s32 clut);
void *drawColorDefault(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color);
s32 drawColorByMenuPalette(s32 renderCtx, s32 cursorY, s32 packedXY, s32 value, s32 color);
void drawMenuColorDefault(s32 a0, s32 a1, s32 a2, s32 a3);

#endif /* BATTLE_RESULTS_NUMBER_H */
