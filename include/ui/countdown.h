#ifndef UI_COUNTDOWN_H
#define UI_COUNTDOWN_H

#include "common.h"
#include "psxsdk/libgpu.h"

extern void setHudBrightness(s32 brightness);
extern void setCountdownVisible(u32 visible);
extern void setCountdownPosition(s32 x, s32 y);
extern void updateCountdownBlink(void);
extern u8 *func_800302DC(void *ot, u8 *pkt);
extern u8 *func_80030518(P_TAG *ot, u8 *pkt);
extern void resetCountdownDisplay(void);

#endif
