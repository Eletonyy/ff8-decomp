#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle_anim.h"
#include "btl_anim.h"
#include "gamestate.h"
#include "numstr.h"
#include "ui/countdown.h"
#include "ui/icon.h"

/**
 * @brief Set the HUD brightness, the grey level of the countdown, the SeeD rank
 * notification and the gauges.
 * @param brightness 0x1000 is full brightness.
 */
void setHudBrightness(s32 brightness) {
    g_engine.countdown.brightness = brightness;
}


/**
 * @brief Show or hide the countdown timer.
 *
 * Showing it restarts the colon blink from the current countdown value.
 *
 * @param visible Nonzero to show the countdown.
 */
void setCountdownVisible(unsigned int visible) {
    CountdownDisplay *disp = &g_engine.countdown;

    disp->visible = visible;
    if (visible != 0) {
        disp->blinkFrames = 0;
        disp->lastSeconds = (s8)g_gameState.mainData.countdownTimer;
    }
}


/**
 * @brief Set where the countdown timer is drawn.
 * @param x Left edge of the minutes.
 * @param y Top edge.
 */
void setCountdownPosition(s32 x, s32 y) {
    CountdownDisplay *disp = &g_engine.countdown;

    disp->x = x;
    disp->y = y;
}


/**
 * @brief Step the countdown's colon blink by one frame.
 *
 * Counts frames up to 0x40 and restarts from 0 whenever the low byte of
 * g_gameState.mainData.countdownTimer changes, so the colon, which is drawn
 * while the count is under half a second, blinks once per second.
 */
void updateCountdownBlink(void) {
    CountdownDisplay *disp = &g_engine.countdown;
    s32 counter;
    s32 clamped;
    s32 gsVal;
    s32 curVal;

    counter = disp->blinkFrames;
    counter++;
    clamped = 0x40;
    if (counter < 0x41U) {
        clamped = counter;
    }
    disp->blinkFrames = clamped;
    gsVal = g_gameState.mainData.countdownTimer;
    curVal = disp->lastSeconds;
    gsVal &= 0xFF;
    counter = gsVal;
    if (counter != curVal) {
        disp->blinkFrames = 0;
        disp->lastSeconds = counter;
    }
}


/**
 * @brief Draw the countdown timer as MM:SS glyphs, then the draw-environment packets.
 *
 * Draws nothing while the countdown is hidden. Otherwise it shows the countdown
 * less one second, clamped to 0x1797 seconds, at the position and brightness in
 * @c g_engine.countdown. A blank leading minutes digit is skipped, and the colon
 * shows for the first half second of each second (30 frames on NTSC, 25 on PAL).
 *
 * @param ot Ordering-table slot the glyphs are linked into.
 * @param pkt First free packet.
 * @return The first free packet after everything drawn.
 */
u8 *drawCountdown(void *ot, u8 *pkt) {
    u8 buf[24];
    u8 *out;
    u32 color;
    s32 timer;
    CountdownDisplay *disp;
    s32 x;
    s32 y;
    s32 i;
    s32 threshold;
    s32 ret;
    s32 c;

    out = pkt;
    ret = GetVideoMode();

    threshold = 30;
    if (ret == MODE_PAL) {
        threshold = 25;
    }

    disp = &g_engine.countdown;
    if (disp->visible == 0) {
        return out;
    }

    color = disp->brightness;
    x = disp->x;
    timer = g_gameState.mainData.countdownTimer;
    y = disp->y;

    color >>= 5;
    color &= 0xFF;

    {
        u32 lo;
        u32 hi;

        lo = color | (color << 8);
        hi = color << 16;
        color = lo | hi;
    }

    color |= SPRT_CODE;
    timer--;
    timer = (timer < 0) ? 0 : ((timer >= 0x1798) ? 0x1797 : timer);

    intToDecStringShort(timer / 60, buf, ICON_BOLD_DIGIT_0);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        if (i != 3 || c != ICON_BOLD_DIGIT_0) {
            out = drawIcon(ot, out, c, x, y, color);
        }
        x += 10;
    }

    x++;
    if (disp->blinkFrames < threshold) {
        out = drawIcon(ot, out, ICON_BOLD_COLON, x, y, color);
    }

    x += 7;
    intToDecStringShort(timer % 60, buf, ICON_BOLD_DIGIT_0);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        out = drawIcon(ot, out, c, x, y, color);
        x += 10;
    }

    return emitDrawEnvPackets(ot, out);
}


/**
 * @brief Draw the countdown timer as MM:SS glyphs.
 *
 * The same display as drawCountdown, read from the same state: the countdown
 * (clamped to 0x1797 seconds) is drawn at the position, brightness and blink phase kept
 * in @c g_engine.countdown, the leading minutes digit is skipped when blank and the
 * colon shows while the blink counter is below 30. Unlike drawCountdown it shows
 * the countdown as is, uses the NTSC blink threshold whatever the video mode and
 * leaves the draw-environment packets to the caller.
 *
 * @param ot Ordering-table slot the glyphs are linked into.
 * @param pkt First free packet.
 * @return The first free packet after the glyphs.
 */
u8 *drawBattleCountdown(P_TAG *ot, u8 *pkt) {
    u8 buf[16];
    u8 *out;
    u32 color;
    s32 timer;
    CountdownDisplay *disp;
    s32 x;
    s32 y;
    s32 i;
    s32 c;

    out = pkt;
    disp = &g_engine.countdown;
    if (disp->visible == 0) {
        return out;
    }

    color = disp->brightness;
    x = disp->x;
    timer = g_gameState.mainData.countdownTimer;
    y = disp->y;

    color >>= 5;
    color &= 0xFF;

    {
        u32 lo;
        u32 hi;

        lo = color | (color << 8);
        hi = color << 16;
        color = lo | hi;
    }

    color |= SPRT_CODE;
    timer = (timer < 0) ? 0 : ((timer >= 0x1798) ? 0x1797 : timer);

    intToDecStringShort(timer / 60, buf, ICON_BOLD_DIGIT_0);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        if (i != 3 || c != ICON_BOLD_DIGIT_0) {
            out = drawIcon(ot, out, c, x, y, color);
        }
        x += 10;
    }

    x++;
    if (disp->blinkFrames < 30) {
        out = drawIcon(ot, out, ICON_BOLD_COLON, x, y, color);
    }

    x += 7;
    intToDecStringShort(timer % 60, buf, ICON_BOLD_DIGIT_0);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        out = drawIcon(ot, out, c, x, y, color);
        x += 10;
    }

    return out;
}


/**
 * @brief Hide the countdown timer and reset its position and blink, with full brightness (0x1000).
 */
void resetCountdownDisplay(void) {
    CountdownDisplay *disp = &g_engine.countdown;

    disp->visible = 0;
    disp->brightness = 0x1000;
    disp->x = 0;
    disp->y = 0;
    disp->lastSeconds = 0;
    disp->blinkFrames = 0;
}
