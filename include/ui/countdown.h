#ifndef UI_COUNTDOWN_H
#define UI_COUNTDOWN_H

#include "common.h"
#include "psxsdk/libgpu.h"

/**
 * @brief The on-screen countdown timer, drawn as MM:SS from
 * @c g_gameState.mainData.countdownTimer.
 */
typedef struct {
    u16 x; /* 0x00 */
    u8 y; /* 0x02 */
    u8 visible; /* 0x03 */
    u16 brightness; /* 0x04: 0x1000 = full; also the SeeD rank notification's and the gauges' grey level */
    u8 blinkFrames; /* 0x06: frames since lastSeconds changed, capped at 0x40 */
    u8 lastSeconds; /* 0x07: low byte of the countdown when it last changed */
} CountdownDisplay;

extern void setHudBrightness(s32 brightness);
extern void setCountdownVisible(u32 visible);
extern void setCountdownPosition(s32 x, s32 y);
extern void updateCountdownBlink(void);
extern u8 *drawCountdown(void *ot, u8 *pkt);
extern u8 *drawBattleCountdown(P_TAG *ot, u8 *pkt);
extern void resetCountdownDisplay(void);

#endif
