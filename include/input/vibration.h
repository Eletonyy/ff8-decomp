#ifndef INPUT_VIBRATION_H
#define INPUT_VIBRATION_H

#include "common.h"

extern s32 isVibrating(void);
extern s32 isVibrationPlaying(s32 cmd);
extern void stopVibration(s32 id);
extern s32 startVibration(u8 *data, s32 idx, s32 priority);
extern void stepVibrations(void);
extern void advanceVibrationClock(s32 delta);
extern void initVibration(void);

#endif
