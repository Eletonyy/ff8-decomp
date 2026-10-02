#ifndef BATTLE_RESULTS_DRAW_H
#define BATTLE_RESULTS_DRAW_H

#include "common.h"
#include "psxsdk/libgpu.h"

void scaleWindowRect(s32 t, RECT *src, RECT *dst);
void drawResultsFrame(void);

#endif /* BATTLE_RESULTS_DRAW_H */
