#ifndef UI_GAUGE_H
#define UI_GAUGE_H

#include "common.h"

extern void stepGauges(void);
extern u8 *drawGauges(void *ot, u8 *pkt);
extern void hideGauge(s32 idx);
extern void setGaugeValue(s32 idx, s32 value);
extern void showGauge(s32 idx, s32 flags, u16 *pos, s32 minValue, s32 maxValue, s32 value);
extern void showGaugeFull(s32 idx, s32 flags, u16 *pos, s32 minValue, s32 maxValue, s32 value, s32 width);
extern void resetGauges(void);

#endif
