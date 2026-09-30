#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "battle_anim.h"
#include "btl_anim.h"
#include "ui/gauge.h"
#include "ui/icon.h"

/* --- Local type definitions --- */

/** @brief A gauge's length in pixels when none is given. */
#define GAUGE_DEFAULT_WIDTH 96

/** @brief The widest sprite a bar is drawn with; a longer bar takes several. */
#define GAUGE_SPRITE_WIDTH 64

/**
 * @brief GP0(E2h) texture-window words: none, the fill's 8x8 tile at (0, 16) and the
 * track's 8x16 tile at (184, 240).
 */
#define TEXWINDOW_OFF 0xE2000000
#define GAUGE_FILL_TEXWINDOW 0xE20103FF
#define GAUGE_TRACK_TEXWINDOW 0xE20F5FDF

/** @brief Palette rows of the red fill and the grey track, in the font's CLUT column. */
#define GAUGE_FILL_CLUT_Y 229
#define GAUGE_TRACK_CLUT_Y 226

/** @brief r, g and b at full: the white the fill blinks to. */
#define GAUGE_BLINK_WHITE 0xFFFFFF

/** @brief A sprite that carries its own draw mode and texture window (tag length 7). */
typedef struct {
    u32 tag;
    u32 drawMode; /* GP0(E1h) */
    u32 texWindow[2]; /* GP0(E2h), then a zero word */
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u32 uvClut; /* u, v and CLUT */
    u16 w, h;
} GaugeSprt;

/* --- Private functions --- */

static s32 lerpRange(s32 rangeStart, s32 rangeEnd, s32 input, s32 maxOut);
static u8 *drawGauge(P_TAG *ot, void *pkt, s32 idx, u32 color);
static void setGaugePosition(s32 idx, u16 *pos);
static void initGauge(s32 idx, s32 flags, u16 *pos, s32 minValue, s32 maxValue, s32 value, s32 width);

/**
 * @brief Perform linear interpolation within a range.
 *
 * Returns 0 if input is below rangeStart, maxOut if at or above rangeEnd,
 * or a proportional value in between.
 *
 * @param rangeStart Minimum input value.
 * @param rangeEnd Maximum input value.
 * @param input Current input value to interpolate.
 * @param maxOut Maximum output value.
 * @return Interpolated value in [0, maxOut].
 */
static s32 lerpRange(s32 rangeStart, s32 rangeEnd, s32 input, s32 maxOut) {
    if (input < rangeStart) {
        return 0;
    }
    if (input >= rangeEnd) {
        return maxOut;
    }
    input -= rangeStart;
    rangeEnd -= rangeStart;
    return input * maxOut / rangeEnd;
}


/**
 * @brief Move each shown gauge's @c fill 2 pixels toward the width its value maps to.
 */
void stepGauges(void) {
    s32 i;
    Gauge *gauge = g_engine.gauges;
    s32 fill;
    s32 target;

    for (i = 0; i < GAUGE_COUNT; i++, gauge++) {
        if (!(gauge->flags & GAUGE_ACTIVE)) {
            continue;
        }

        fill = gauge->fill;
        target = lerpRange(gauge->minValue, gauge->maxValue, gauge->value, gauge->width);

        if (fill < target) {
            fill += 2;
            if (target < fill) {
                fill = target;
            }
        }
        if (target < fill) {
            fill -= 2;
            if (fill < target) {
                fill = target;
            }
        }

        gauge->fill = fill;
    }
}


/**
 * @brief Draw one gauge.
 *
 * Only while it is @ref GAUGE_ACTIVE. The gauge sits at its position: the end caps
 * at x + 8 and x + width + 8, a draw-mode packet that turns the texture window
 * off, then the bar as sprites up to @ref GAUGE_SPRITE_WIDTH wide: its filled
 * part (@c fill pixels, 8 high, 3 lines down) over a 16-high background @c width
 * pixels long. With @ref GAUGE_BLINK_LOW set, the filled part blinks white
 * (@ref GAUGE_BLINK_ON toggling each call) while the value maps to at most a
 * quarter of the width.
 *
 * @param ot Ordering-table slot the packets are linked into.
 * @param pkt First free packet.
 * @param idx Gauge index (0 or 1).
 * @param color Colour word of the icons, the background and the bar.
 * @return The packet cursor after emitDrawEnvPackets.
 */
static u8 *drawGauge(P_TAG *ot, void *pkt, s32 idx, u32 color) {
    Gauge *gauge;
    DR_TWIN *tw;
    GaugeSprt *p;
    u32 barColor;
    s32 flags;
    s32 originX;
    s32 originY;
    u32 link; /* Regalloc: one link temp per addPrim, each live from entry; this one is spilled. */
    u32 link2;
    u32 link3;
    s32 x;
    s32 y;
    s32 n;
    s32 w;

    p = pkt;
    gauge = g_engine.gauges;
    gauge = &gauge[idx];
    flags = gauge->flags;
    originX = gauge->x;
    originY = gauge->y;
    if (flags & GAUGE_ACTIVE) {
        barColor = color;
        if (flags & GAUGE_BLINK_LOW) {
            if (lerpRange(gauge->minValue, gauge->maxValue, gauge->value, gauge->width) <= gauge->width / 4) {
                flags ^= GAUGE_BLINK_ON;
                if (flags & GAUGE_BLINK_ON) {
                    barColor = color | GAUGE_BLINK_WHITE;
                }
                gauge->flags = flags;
            }
        }
        x = originX;
        y = originY;
        p = drawIcon(ot, p, ICON_GAUGE_LEFT_CAP, x + 8, y, color);
        p = drawIcon(ot, p, ICON_GAUGE_RIGHT_CAP, x + gauge->width + 8, y, color);
        tw = (DR_TWIN *)p;
        setlen(tw, 2);
        tw->code[0] = TEXWINDOW_OFF;
        tw->code[1] = 0;
        addPrimFastWithTempOperand(ot, tw, link);
        p = (GaugeSprt *)(tw + 1);

        x += 8;
        y += 3;
        for (n = gauge->fill; n > 0; n -= GAUGE_SPRITE_WIDTH) {
            w = n;
            if (w > GAUGE_SPRITE_WIDTH) {
                w = GAUGE_SPRITE_WIDTH;
            }
            *(u32 *)&p->r0 = barColor; /* r, g, b and code in one store; the code is set again below */
            setlen(p, 7);
            p->drawMode = _get_mode(1, 0, getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y));
            p->texWindow[0] = GAUGE_FILL_TEXWINDOW;
            p->texWindow[1] = 0;
            p->code = SPRT_CODE >> SPRT_CODE_SHIFT;
            setXY0(p, x, y);
            setWH(p, w, 8);
            p->uvClut = getClut(ICON_CLUT_X, GAUGE_FILL_CLUT_Y) << 16;
            addPrimFastWithTempOperand(ot, p, link2);
            p++;
            x += GAUGE_SPRITE_WIDTH;
        }

        x = originX + 8;
        y = originY;
        for (n = gauge->width; n > 0; n -= GAUGE_SPRITE_WIDTH) {
            w = n;
            if (w > GAUGE_SPRITE_WIDTH) {
                w = GAUGE_SPRITE_WIDTH;
            }
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->drawMode = _get_mode(1, 0, getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y));
            p->texWindow[0] = GAUGE_TRACK_TEXWINDOW;
            p->texWindow[1] = 0;
            p->code = SPRT_CODE >> SPRT_CODE_SHIFT;
            setXY0(p, x, y);
            setWH(p, w, 16);
            p->uvClut = getClut(ICON_CLUT_X, GAUGE_TRACK_CLUT_Y) << 16;
            addPrimFastWithTempOperand(ot, p, link3);
            p++;
            x += GAUGE_SPRITE_WIDTH;
        }
    }
    return emitDrawEnvPackets(ot, (u8 *)p); /* p itself: copying it back into pkt costs an instruction */
}


/**
 * @brief Draw both gauges, grey at the HUD brightness.
 *
 * @param ot OT base pointer.
 * @param pkt Packet buffer pointer.
 * @return Updated packet pointer.
 */
u8 *drawGauges(void *ot, u8 *pkt)
{
    u8 *ret = pkt;
    s32 i;
    u32 packed = g_engine.countdown.brightness;

    u32 t = packed >> 5;
    u32 hi = t << 16;
    u32 lo = (t << 8) | SPRT_CODE;

    packed = (hi | lo) | t;

    for (i = 0; i < GAUGE_COUNT; i++) {
        ret = drawGauge(ot, ret, i, packed);
    }

    return ret;
}


/**
 * @brief Hide a gauge.
 * @param idx Gauge index (0 or 1).
 */
void hideGauge(s32 idx) {
    Gauge *gauge = g_engine.gauges;
    gauge = &gauge[idx];
    gauge->flags &= ~GAUGE_ACTIVE;
}


/**
 * @brief Set a gauge's value.
 *
 * With @ref GAUGE_SNAP set the fill jumps to the new value's width; otherwise
 * stepGauges moves it there.
 *
 * @param idx Gauge index (0 or 1).
 * @param value The new value.
 */
void setGaugeValue(s32 idx, s32 value) {
    Gauge *gauge = g_engine.gauges;
    s32 result;

    gauge = &gauge[idx];
    result = lerpRange(gauge->minValue, gauge->maxValue, value, gauge->width);
    gauge->value = value;
    if (gauge->flags & GAUGE_SNAP) {
        gauge->fill = result;
    }
}


/**
 * @brief Set a gauge's position.
 *
 * @param idx Gauge index (0 or 1).
 * @param pos The x and y to copy.
 */
static void setGaugePosition(s32 idx, u16 *pos) {
    Gauge *gauge = g_engine.gauges;
    gauge = &gauge[idx];
    memcpy(&gauge->x, pos, 4);
}


/**
 * @brief Show a gauge, filled to its value at once.
 *
 * @param idx Gauge index (0 or 1).
 * @param flags GAUGE_* flags; @ref GAUGE_ACTIVE is added.
 * @param pos The x and y to copy.
 * @param minValue The value of an empty bar.
 * @param maxValue The value of a full bar.
 * @param value The value to show.
 * @param width The bar's length in pixels.
 */
static void initGauge(s32 idx, s32 flags, u16 *pos, s32 minValue, s32 maxValue, s32 value, s32 width) {
    Gauge *gauge = g_engine.gauges;
    s32 activeFlags = flags | GAUGE_ACTIVE;

    gauge = &gauge[idx];
    setGaugePosition(idx, pos);
    gauge->flags = activeFlags;
    gauge->minValue = minValue;
    gauge->maxValue = maxValue;
    gauge->value = value;
    gauge->width = width;
    gauge->fill = lerpRange(minValue, maxValue, value, width);
}



/**
 * @brief Show a gauge @ref GAUGE_DEFAULT_WIDTH pixels long; see initGauge.
 */
void showGauge(s32 idx, s32 flags, u16 *pos, s32 minValue, s32 maxValue, s32 value) {
    initGauge(idx, flags & 0xFF, pos, minValue, maxValue, value, GAUGE_DEFAULT_WIDTH);
}


/**
 * @brief Show a gauge of any length; see initGauge.
 */
void showGaugeFull(s32 idx, s32 flags, u16 *pos, s32 minValue, s32 maxValue, s32 value, s32 width) {
    initGauge(idx, flags & 0xFF, pos, minValue, maxValue, value, width);
}


/**
 * @brief Hide both gauges.
 */
void resetGauges(void) {
    Gauge *gauge = g_engine.gauges;
    s32 i;

    for (i = 0; i < GAUGE_COUNT; i++, gauge++) {
        gauge->flags = 0;
    }
}
