#ifndef BATTLE_RESULTS_DISPLAY_H
#define BATTLE_RESULTS_DISPLAY_H

#include "common.h"
#include "psxsdk/libgpu.h"

/**
 * @brief One buffer of the battle results screen's display: its display and draw
 * environments, a 2-entry ordering table and its packet space.
 */
typedef struct {
    DISPENV disp; /* 0x00 */
    DRAWENV draw; /* 0x14 */
    u32 ot[2]; /* 0x70: 2-entry ordering table */
    void *pktAlloc; /* 0x78: current packet allocation pointer */
    u32 pktBase; /* 0x7C: packet buffer start */
} ResultsDisplay;

extern ResultsDisplay *g_resultsDisplay; /* 0x80083918: the buffer being drawn */
extern ResultsDisplay *g_resultsDisplays[]; /* 0x80083920: the pair */

extern u8 *getThreadStackTop(void);
extern void waitGpuIdle(void);
extern u32 getResultsDisplayBase(void);
extern s32 getResultsDisplaySize(void);
extern void flipResultsDisplay(void);
extern void submitResultsDisplay(void);
extern void initResultsDisplays(void);
extern void clearResultsDisplay(void);

#endif
