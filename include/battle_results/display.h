#ifndef BATTLE_RESULTS_DISPLAY_H
#define BATTLE_RESULTS_DISPLAY_H

#include "common.h"

extern u8 *getThreadStackTop(void);
extern void waitGpuIdle(void);
extern u32 getResultsDisplayBase(void);
extern s32 getResultsDisplaySize(void);
extern void flipResultsDisplay(void);
extern void submitResultsDisplay(void);
extern void initResultsDisplays(void);
extern void clearResultsDisplay(void);

#endif
