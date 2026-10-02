#include "common.h"
#include "battle_results/draw.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "battle_results/display.h"
#include "battle_results/number.h"
#include "battle_results/result.h"
#include "battle_results/update.h"
#include "ui/window.h"
#include "ui/font.h"
#include "ui/text.h"
#include "btl_anim.h"
#include "game.h"
#include "input/button_remap.h"
#include "menu_tint.h"
#include "ui/icon.h"
#include "numstr.h"

#define MAX(a, b) ((a) < (b) ? (b) : (a))

/* --- Private functions --- */

static DR_AREA *resetDrawArea(P_TAG *ot, DR_AREA *prim);
static DR_AREA *setDrawAreaRect(P_TAG *ot, DR_AREA *prim, RECT *rect);
static DR_AREA *setInsetDrawArea(P_TAG *ot, DR_AREA *prim, RECT *rect);
static DR_AREA *drawWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *drawWindowShifted(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static s32 slideByProgress(s32 x, s32 w, s32 dir, s32 t);
static DR_AREA *drawTextWipe(P_TAG *ot, DR_AREA *prim, u8 *fromMsg, u8 *toMsg, s32 color);
static DR_AREA *drawRewardWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *drawTitleWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *drawRewardDescWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *drawPromptWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *drawMessageWindow(P_TAG *ot, DR_AREA *prim, s32 color);
static DR_AREA *drawItemsPage(P_TAG *ot, DR_AREA *prim);
static DR_AREA *drawExpRow(P_TAG *ot, DR_AREA *prim, s32 idx, s32 x, s32 y, s32 t, s32 color);
static DR_AREA *drawExpPage(P_TAG *ot, DR_AREA *prim, s32 color);
static DR_AREA *drawGfLevelUpWindow(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 gf);
static DR_AREA *drawGfLearnedWindow(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 gf, s32 ability);
static DR_AREA *drawGfApWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color, s32 ap);

/**
 * @brief Emit a @c DR_AREA primitive into the OT and advance the packet.
 *
 * Loads the current frame's drawing-area GP0 commands from the scratchpad
 * @c ResultsScreen staged by @c drawResultsFrame, packs them into the
 * @c DR_AREA primitive at @c prim, links @c prim into @c ot's slot chain
 * via @c addPrimFast (temp $a3), and returns the next packet cursor.
 *
 * @param ot   OT slot pointer.
 * @param prim Storage for the new primitive (must have space for one DR_AREA).
 * @return Cursor for the next primitive (@c prim @c + @c 1).
 */
static DR_AREA *resetDrawArea(P_TAG *ot, DR_AREA *prim) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u32 c0 = ctx->drawAreaTL;
    u32 c1 = ctx->drawAreaBR;
    setlen(prim, 2);
    prim->code[0] = c0;
    prim->code[1] = c1;
    addPrimFast(ot, prim, a3);
    return prim + 1;
}


/**
 * @brief Scale a rect about its centre by an eased animation progress.
 *
 * At |@p t| = 0x1000 @p dst is a copy of @p src. Otherwise @p dst keeps src's
 * centre and is src's size times a curve value of 0..64, over 64. A positive
 * @p t grows along 64 - g_animCurveFadeIn (fast start), any other along
 * g_animCurveFadeOut (slow start). @p src and @p dst may be the same rect.
 *
 * @param t Progress, -0x1000..0x1000; the sign picks the curve.
 * @param src Full-size rect.
 * @param dst Receives the scaled rect.
 */
void scaleWindowRect(s32 t, RECT *src, RECT *dst) {
    s32 a;
    s32 x;
    s32 y;
    u32 w;
    u32 h;
    s32 scale;

    a = abs(t);
    if (a == 0x1000) {
        *dst = *src;
        return;
    }
    x = src->x;
    y = src->y;
    w = src->w;
    h = src->h;
    x += w >> 1;
    y += h >> 1;
    if (t > 0) {
        scale = g_animCurveFadeIn[t >> 6];
        scale = 64 - scale;
    } else {
        scale = g_animCurveFadeOut[a / 64];
    }
    w = (scale * w) >> 7;
    h = (scale * h) >> 7;
    dst->x = x - w;
    dst->y = y - h;
    dst->w = w * 2;
    dst->h = h * 2;
}


/**
 * @brief Emit a @c DR_AREA for @p rect, relative to the buffer being drawn.
 *
 * Offsets a copy of @p rect by the VRAM origin in the scratchpad
 * @c ResultsScreen, keeps it at least 2x2, packs it into @p prim with
 * @c SetDrawArea and links @p prim into @p ot via @c addPrimFast (temp $s1).
 *
 * @param ot OT slot pointer.
 * @param prim Storage for the new primitive.
 * @param rect Draw area, relative to the buffer's origin.
 * @return Cursor for the next primitive (@c prim @c + @c 1).
 */
static DR_AREA *setDrawAreaRect(P_TAG *ot, DR_AREA *prim, RECT *rect) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    RECT r;

    r = *rect;
    r.x += ctx->originX;
    r.y += ctx->originY;
    if (r.w < 2) {
        r.w = 2;
    }
    if (r.h < 2) {
        r.h = 2;
    }
    SetDrawArea(prim, &r);
    addPrimFast(ot, prim, s1);
    return prim + 1;
}


/**
 * @brief Emit a @c DR_AREA (setDrawAreaRect) for @p rect shrunk by a pixel on
 * each side.
 *
 * Shrinks @p rect in place for the call and restores it after.
 *
 * @param ot OT slot pointer.
 * @param prim Storage for the primitive.
 * @param rect Draw area to shrink, relative to the buffer's origin.
 * @return Cursor for the next primitive.
 */
static DR_AREA *setInsetDrawArea(P_TAG *ot, DR_AREA *prim, RECT *rect) {
    /* x/y and w/h are saved as one word each, as in the binary; a RECT copy
       would go through the stack. */
    s32 save0 = *(s32 *)&rect->x;
    s32 save1 = *(s32 *)&rect->w;
    rect->x += 1;
    rect->y += 1;
    rect->w -= 2;
    rect->h -= 2;
    prim = setDrawAreaRect(ot, prim, rect);
    *(s32 *)&rect->x = save0;
    *(s32 *)&rect->w = save1;
    return prim;
}


/**
 * @brief Draw the window at ResultsScreen::rect, scaled by its open/close animation.
 *
 * Scales @c rect by |@p t| into @c animRect (scaleWindowRect), then links the
 * frame (func_8002B898) on the scaled rect and the background (func_8002B8BC)
 * on the full one. Mid-animation (|@p t| < 0x1000) it brackets them with draw
 * areas on the scaled rect, the first one inset by a pixel. Draws nothing when
 * @p t is 0.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Animation progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the frame and background.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 a;

    a = abs(t);
    if (a != 0) {
        scaleWindowRect(a, &ctx->rect, &ctx->animRect);
        if (a < 0x1000) {
            prim = setInsetDrawArea(ot, prim, &ctx->animRect);
        }
        prim = func_8002B898(ot, prim, &ctx->animRect, color);
        prim = func_8002B8BC(ot, prim, &ctx->rect, color, 0);
        if (a < 0x1000) {
            prim = setDrawAreaRect(ot, prim, &ctx->animRect);
        }
    }
    return prim;
}


/**
 * @brief drawWindow with the window's background texture shifted 8 texels along x
 * (func_8002B8BC's last argument; drawWindow passes 0).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Animation progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the frame and background.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawWindowShifted(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 a;

    a = abs(t);
    if (a != 0) {
        scaleWindowRect(a, &ctx->rect, &ctx->animRect);
        if (a < 0x1000) {
            prim = setInsetDrawArea(ot, prim, &ctx->animRect);
        }
        prim = func_8002B898(ot, prim, &ctx->animRect, color);
        prim = func_8002B8BC(ot, prim, &ctx->rect, color, 8);
        if (a < 0x1000) {
            prim = setDrawAreaRect(ot, prim, &ctx->animRect);
        }
    }
    return prim;
}


/**
 * @brief Slide a coordinate by an eased share of a width.
 *
 * Moves @p x by (@p w - 8) * g_animCurveFadeIn[@p t / 64] / 64, adding when
 * @p dir is set and subtracting otherwise. The curve falls from 64, so the
 * offset shrinks from w - 8 towards 0 as @p t runs from 0 to 0xFFF.
 *
 * @param x Coordinate to move.
 * @param w Width; the offset is taken from w - 8.
 * @param dir Nonzero to add the offset, zero to subtract it.
 * @param t Progress, 0..0xFFF.
 * @return The moved coordinate.
 */
static s32 slideByProgress(s32 x, s32 w, s32 dir, s32 t) {
    s32 d = w - 8;
    if (dir != 0) {
        w = t / 64;
        w = g_animCurveFadeIn[w];
        d = d * w / 64;
        x += d;
    } else {
        w = t / 64;
        w = g_animCurveFadeIn[w];
        d = d * w / 64;
        x -= d;
    }
    return x;
}


/**
 * @brief Wipe one message into another behind a sliding divider.
 *
 * As @c wipeProgress grows, an 8-pixel divider slides right across the scratchpad
 * window rect along g_animCurveFadeIn (slideByProgress). The pane left of it
 * shows @p toMsg moving in with the divider; the pane right of it keeps
 * @p fromMsg in place. Each pane is clipped to its part of the window
 * (setInsetDrawArea) and skipped once it has no width; text sits 9 pixels below
 * the rect's top. The divider is drawn last with func_8002B3A0 in @p color.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param fromMsg Message being wiped out, or NULL.
 * @param toMsg Message being wiped in, or NULL.
 * @param color Colour word for the divider.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawTextWipe(P_TAG *ot, DR_AREA *prim, u8 *fromMsg, u8 *toMsg, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u8 text[0x100];
    u8 gpArea[0x300];
    u8 *area;
    u8 *tempGp;
    u8 *savedGp;
    u8 *ret;
    s32 split;
    s32 w;
    s32 y;
    s32 x;
    s32 tx;

    prim = resetDrawArea(ot, prim);
    split = slideByProgress(ctx->rect.x, ctx->rect.w, 0, ctx->wipeProgress);
    y = ctx->rect.y + 9;
    w = split + ctx->rect.w - ctx->rect.x;
    ctx->animRect.y = ctx->rect.y;
    ctx->animRect.h = ctx->rect.h;
    if (w > 0) {
        tx = split + 10;
        if (toMsg != NULL) {
            area = gpArea;
            GP_SAVE_SET(tempGp, area);
            savedGp = tempGp;
            decodeMessage(toMsg, text, -1);
            GP_RESTORE_RET(savedGp, ret);
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, y, text, 7);
        }
        ctx->animRect.x = ctx->rect.x;
        ctx->animRect.w = w;
        prim = setInsetDrawArea(ot, prim, &ctx->animRect);
    }
    x = ctx->rect.x + w;
    x += 8;
    w = ctx->rect.x + ctx->rect.w;
    w -= x;
    if (w > 0) {
        tx = ctx->rect.x + 10;
        if (fromMsg != NULL) {
            area = gpArea;
            GP_SAVE_SET(tempGp, area);
            savedGp = tempGp;
            decodeMessage(fromMsg, text, -1);
            GP_RESTORE_RET(savedGp, ret);
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, y, text, 7);
        }
        ctx->animRect.x = x;
        ctx->animRect.w = w;
        prim = setInsetDrawArea(ot, prim, &ctx->animRect);
    }
    ctx->animRect.w = 8;
    ctx->animRect.x = ctx->rect.w + split - 8;
    prim = func_8002B3A0(ot, prim, &ctx->animRect, color, 2);
    return prim;
}


/**
 * @brief Draw the item reward line in its window at (0x48, 0x47).
 *
 * Draws nothing when @p t is 0. Otherwise links the frame icons and bars,
 * then, while @c rewardCount is set, the current reward's name and count: wiped in
 * from the previous reward's name (drawTextWipe) while @c wipeProgress runs, or
 * plain otherwise. The window itself is drawn last, scaled by @p t
 * (drawWindowShifted).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the frame, text and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawRewardWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 slide;
    s32 x;
    s32 y;

    if (t == 0) {
        return prim;
    }
    prim = resetDrawArea(ot, prim);
    x = 0x48;
    y = 0x47;
    prim = drawIcon(ot, prim, 0x4C, x, y, color);
    prim = drawIcon(ot, prim, 0x4D, x + 0xC2, y, color);
    ctx->rect.x = x + 0xBE;
    ctx->rect.y = y;
    ctx->rect.w = 8;
    ctx->rect.h = 0x1A;
    prim = func_8002B3A0(ot, prim, &ctx->rect, color, 1);
    ctx->rect.x = x + 0xB6;
    ctx->rect.y = y;
    ctx->rect.w = 8;
    ctx->rect.h = 0x1A;
    prim = func_8002B3A0(ot, prim, &ctx->rect, color, 2);
    slide = ctx->wipeProgress;
    if (ctx->rewardCount != 0) {
        if (slide != 0) {
            ctx->rect.x = x;
            ctx->rect.y = y;
            ctx->rect.w = 0xBD;
            ctx->rect.h = 0x1A;
            formatRewardName(ctx->reward[-1].id, ctx->text[0]);
            formatRewardName(ctx->reward[0].id, ctx->text[1]);
            prim = drawTextWipe(ot, prim, ctx->text[0], ctx->text[1], color);
            prim = drawNumberDefault(ot, (SPRT *)prim, ((y + 9) << 16) | (x + 0xE6), ctx->reward[-1].count, color);
        } else {
            formatRewardName(ctx->reward[0].id, ctx->text[2]);
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 10, y + 9, ctx->text[2], 7);
            prim = drawNumberDefault(ot, (SPRT *)prim, ((y + 9) << 16) | (x + 0xE6), ctx->reward[0].count, color);
        }
    }
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = 0xF0;
    ctx->rect.h = 0x1A;
    return drawWindowShifted(ot, prim, t, color);
}


/**
 * @brief Draw the title window: icon 0x57 and the @c title message.
 *
 * Draws nothing when @p t is 0. The window (0x10, 8, 0x160 x 0x1A) is drawn
 * last, scaled by @p t (drawWindow).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the icon and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawTitleWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 y;

    if (t == 0) {
        return prim;
    }
    prim = resetDrawArea(ot, prim);
    y = 8;
    prim = drawIcon(ot, prim, 0x57, 0x10, y, color);
    if (ctx->title != NULL) {
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, 0x1A, 0x11, ctx->title, 7);
    }
    ctx->rect.x = 0x10;
    ctx->rect.y = y;
    ctx->rect.w = 0x160;
    ctx->rect.h = 0x1A;
    return drawWindow(ot, prim, t, color);
}


/**
 * @brief Draw the description window for the item reward on display.
 *
 * Draws nothing when @p t is 0. Otherwise links icon 0x55 and, while
 * @c rewardCount is set, the reward's description (getRewardDesc; cards have
 * none): wiped in from the previous reward's (drawTextWipe) while @c wipeProgress
 * runs, or decoded and drawn plain otherwise. The window (0x10, 0x9C,
 * 0x160 x 0x1A) is drawn last, scaled by @p t (drawWindow).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the icon, text and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawRewardDescWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u8 text[0x100];
    u8 gpArea[0x300];
    u8 *area;
    u8 *tempGp;
    u8 *savedGp;
    u8 *ret;
    u8 *from;
    u8 *to;
    u8 *msg;
    s32 slide;
    s32 x;
    s32 y;

    if (t == 0) {
        return prim;
    }
    prim = resetDrawArea(ot, prim);
    x = 0x10;
    y = 0x9C;
    prim = drawIcon(ot, prim, 0x55, x, y, color);
    slide = ctx->wipeProgress;
    if (ctx->rewardCount != 0) {
        if (slide != 0) {
            ctx->rect.x = x;
            ctx->rect.y = y;
            ctx->rect.w = 0x160;
            ctx->rect.h = 0x1A;
            from = getRewardDesc(ctx->reward[-1].id);
            to = getRewardDesc(ctx->reward[0].id);
            if (from != to || from != NULL) {
                prim = drawTextWipe(ot, prim, from, to, color);
            }
        } else {
            msg = getRewardDesc(ctx->reward[0].id);
            if (msg != NULL) {
                area = gpArea;
                GP_SAVE_SET(tempGp, area);
                savedGp = tempGp;
                decodeMessage(msg, text, -1);
                GP_RESTORE_RET(savedGp, ret);
                prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 10, y + 7, text, 7);
            }
        }
    }
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = 0x160;
    ctx->rect.h = 0x1A;
    return drawWindow(ot, prim, t, color);
}


/**
 * @brief Draw the button prompt window: a button icon and the @c prompt text.
 *
 * Draws nothing when @p t is 0. The icon (reverseButtonRemap(6) + 0x80) and
 * the first line of @c prompt are centred together along y = 0xC0. The window
 * (0x10, 0xB8, 0x160 x 0x1A) is drawn last, scaled by @p t (drawWindow),
 * and the full draw area is restored after it.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the icon and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawPromptWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u8 *msg;
    s32 x;
    s32 icon;

    if (t == 0) {
        return prim;
    }
    prim = resetDrawArea(ot, prim);
    msg = ctx->prompt;
    if (msg != NULL) {
        x = (0x152 - getFirstLineWidth(msg)) / 2 + 0x10;
        icon = reverseButtonRemap(6);
        icon += 0x80;
        prim = drawIcon(ot, prim, icon, x, 0xC0, color);
        x += getIconWidth(icon);
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x, 0xC0, msg, 7);
    }
    ctx->rect.x = 0x10;
    ctx->rect.y = 0xB8;
    ctx->rect.w = 0x160;
    ctx->rect.h = 0x1A;
    prim = drawWindow(ot, prim, t, color);
    return resetDrawArea(ot, prim);
}


/**
 * @brief Draw the message window: message @c message in a box centred on screen.
 *
 * Only while @c unk44 is set and the window's progress @c messageProgress is nonzero.
 * Formats @c message with @c messageReward into the scratchpad's fourth text buffer
 * (formatMessage), sizes a window around it (text plus 0x14 x 0xE), centres
 * it on x 0xC0, y 0x7E, then links icon 0x56, the text and the window,
 * scaled by @c messageProgress (drawWindowShifted).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param color Colour word for the icon and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawMessageWindow(P_TAG *ot, DR_AREA *prim, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u32 size;
    u32 w;
    u32 h;
    s32 x;
    s32 y;
    s32 t;
    s32 active;

    active = ctx->unk44;
    if (active == 0) {
        return prim;
    }
    if (ctx->messageProgress == 0) {
        return prim;
    }
    formatMessage(ctx->message, ctx->text[3], ctx->messageReward, 0, 0, 0, 0);
    size = getTextSize(ctx->text[3]);
    h = size >> 16;
    w = size & 0xFFFF;
    h += 0xE;
    w += 0x14;
    x = (0x180 - w) >> 1;
    y = 0x7E - (h >> 1);
    ctx->rect.x = x;
    ctx->rect.w = w;
    ctx->rect.y = y;
    ctx->rect.h = h;
    t = ctx->messageProgress;
    prim = drawIcon(ot, prim, 0x56, x, y, color);
    prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 10, y + 7, ctx->text[3], 7);
    return drawWindowShifted(ot, prim, t, color);
}


/**
 * @brief Draw the item page: the message and description windows at
 * @c descProgress, the reward window at @c pageProgress.
 *
 * A negative progress fades: the menu brightness and the windows'
 * grey colour follow |progress|; otherwise both are at full (0x1000, 0x808080).
 * A window group at progress 0 is not drawn.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawItemsPage(P_TAG *ot, DR_AREA *prim) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 t;
    s32 b;
    u32 color;

    t = ctx->descProgress;
    if (t < 0) {
        b = abs(t);
        /* abs() is never negative here, but the binary has this test. */
        if (b < 0) {
            b = 0;
        }
        color = b;
        setMenuBrightness(b);
        color >>= 5;
        color &= 0xFF;
        color = color | (color << 8) | (color << 16);
    } else {
        color = 0x808080;
        setMenuBrightness(0x1000);
    }
    color |= 0x64000000;
    if (t != 0) {
        prim = drawMessageWindow(ot, prim, color);
        prim = drawRewardDescWindow(ot, prim, t, color);
    }
    t = ctx->pageProgress;
    if (t < 0) {
        color = abs(t);
        setMenuBrightness(color);
        color >>= 5;
        color &= 0xFF;
        color = color | (color << 8) | (color << 16);
    } else {
        color = 0x808080;
        setMenuBrightness(0x1000);
    }
    color |= 0x64000000;
    if (t != 0) {
        prim = drawRewardWindow(ot, prim, t, color);
    }
    return prim;
}


/**
 * @brief Draw one party member's row of the EXP page.
 *
 * Only a row with a name: first its "LEVEL UP!" popup (menu string 0x30)
 * while @c levelUpTimer counts (progress @c levelUpTimer << 9), and the
 * centred "Didn't receive EXP" popup (menu string 0x31) while @c noExpPopup is
 * 1..0x40 (progress << 6). Then the name in its colour, the labels "EXP
 * Acquired", "Current EXP" and "Next LEVEL" (menu strings 0x1D..0x1F) with
 * @c expAcquired, @c currentExp and @c nextLevelExp as 9-digit numbers and
 * icon 0xC after each, then icon 0xE and the row's @c level. The row's window
 * is drawn last, scaled by @p t.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param idx Row (character) index, 0..2.
 * @param x Left of the row.
 * @param y Top of the row.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the icons and windows.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawExpRow(P_TAG *ot, DR_AREA *prim, s32 idx, s32 x, s32 y, s32 t, s32 color) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u8 buf[16];
    s32 digit;
    s32 blank;
    s32 progress;
    s32 popup;
    s32 tx;
    s32 ty;
    s32 col;
    u8 *str;

    digit = getMenuString(0xB)[1];
    blank = getMenuString(0xB)[0];
    if (ctx->names[idx] != NULL) {
        if (ctx->levelUpTimer[idx] != 0) {
            prim = resetDrawArea(ot, prim);
            col = 7;
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 0x5D, y + 8, getMenuString(0x30), col);
            ctx->rect.x = x + 0x53;
            ctx->rect.w = ctx->levelUpWidth;
            ctx->rect.y = y + 4;
            ctx->rect.h = 0x14;
            progress = ctx->levelUpTimer[idx] << 9;
            prim = drawWindow(ot, prim, progress < 0 ? 0 : progress > 0x1000 ? 0x1000 : progress, color);
        }
        popup = ctx->noExpPopup[idx];
        if (popup != 0 && popup < 0x41) {
            prim = resetDrawArea(ot, prim);
            col = 7;
            str = getMenuString(0x31);
            tx = (0x194 - ctx->noExpWidth) / 2;
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, y + 8, str, col);
            ctx->rect.x = tx - 10;
            ctx->rect.y = y + 4;
            ctx->rect.h = 0x14;
            ctx->rect.w = ctx->noExpWidth + 0x14;
            progress = ctx->noExpPopup[idx] << 6;
            prim = drawWindow(ot, prim, progress < 0 ? 0 : progress > 0x1000 ? 0x1000 : progress, color);
        }
        col = ctx->nameColor[idx];
        tx = x + 10;
        ty = y + 7;
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, ty, ctx->names[idx], col);
        col = 7;
        tx = x + 0xA6;
        str = getMenuString(0x1D);
        ty = y + 5;
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, ty, str, col);
        str = getMenuString(0x1E);
        ty = y + 0x14;
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, ty, str, col);
        str = getMenuString(0x1F);
        ty = y + 0x23;
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, ty, str, col);
        tx = x + 0xE3;
        ty = y + 7;
        str = buf;
        intToDecString(ctx->expAcquired[idx], str, digit);
        replaceLeadingZeros(str, 9, digit, blank);
        prim = func_8002C56C(ot, prim, tx, ty, str, col);
        tx += 0x50;
        prim = drawIcon(ot, prim, 0xC, tx, ty, color);
        tx = x + 0xE3;
        ty = y + 0x16;
        intToDecString(ctx->currentExp[idx], str, digit);
        replaceLeadingZeros(str, 9, digit, blank);
        prim = func_8002C56C(ot, prim, tx, ty, str, col);
        tx += 0x50;
        prim = drawIcon(ot, prim, 0xC, tx, ty, color);
        tx = x + 0xE3;
        ty = y + 0x25;
        intToDecString(ctx->nextLevelExp[idx], str, digit);
        replaceLeadingZeros(str, 9, digit, blank);
        prim = func_8002C56C(ot, prim, tx, ty, str, col);
        tx += 0x50;
        prim = drawIcon(ot, prim, 0xC, tx, ty, color);
        tx = x + 10;
        ty = y + 0x12;
        prim = drawIconClut(ot, (TSPRT *)prim, 0xE, tx, ty, color, 0x1C2);
        tx = x + 0x3E;
        ty = y + 0x14;
        prim = drawNumber(ot, (SPRT *)prim, (ty << 16) | (tx & 0xFFFF), ctx->level[idx], color, col);
    }
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = 0x143;
    ctx->rect.h = 0x32;
    return drawWindow(ot, prim, t, color);
}


/**
 * @brief Draw the EXP page: the three party rows (drawExpRow), 0x32 apart,
 * fully open.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor, threaded through each call.
 * @param color Colour word passed to each row.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawExpPage(P_TAG *ot, DR_AREA *prim, s32 color) {
    s32 x = 0x1E;
    s32 y = 0x22;
    s32 i = 0;
    s32 t = 0x1000;
    do {
        prim = drawExpRow(ot, prim, i, x, y, t, color);
        i++;
        y += 0x32;
    } while (i < 3);
    return prim;
}


/**
 * @brief Draw the one-line "GF <name> LEVEL UP!" window, centred, at row @p y.
 *
 * The text is "GF " (menu string 0x79), the GF's name, a blank (the first
 * character of menu string 0xB) and "LEVEL UP!" (menu string 0x30). The
 * window fits the text's first line plus 8 pixels on each side and is drawn
 * scaled by @p t (drawWindow).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor; the text's sprites come first.
 * @param y Top of the window.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the window.
 * @param gf GF index; its name is magic name @p gf + 0x40.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawGfLevelUpWindow(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 gf) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u8 text[0x80];
    u8 sep[2];
    s32 w;
    s32 x;

    sep[0] = getMenuString(0xB)[0];
    sep[1] = 0;
    copyString(text, getMenuString(0x79));
    btlStrcat2(text, getMagicNamePtr(gf + 0x40));
    btlStrcat2(text, sep);
    btlStrcat2(text, getMenuString(0x30));
    w = getFirstLineWidth(text);
    x = (0x170 - w) / 2;
    prim = drawDecodedText(ot, prim, x + 8, y + 7, text, 7);
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = w + 0x10;
    ctx->rect.h = 0x1A;
    return drawWindow(ot, (DR_AREA *)prim, t, color);
}


/**
 * @brief Draw the three-line "GF <name> / learned / <ability>!" window.
 *
 * The first line is "GF " (menu string 0x79) and the GF's name. The second
 * is "learned" (menu string 0x7F), indented by 12. The third is the icon of
 * the ability's category (the ranges of getAbilityCategory) and the ability's
 * name followed by "!" (menu string 0x7E), indented by 24. The window fits
 * the widest line, is centred in 384 and is drawn scaled by @p t (drawWindow).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor; the text and icon sprites come first.
 * @param y Top of the window.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the icon and window.
 * @param gf GF index; its name is magic name @p gf + 0x40.
 * @param ability Ability id.
 * @return Packet cursor after the primitives.
 * @note The two indents are variables set before the width is computed and
 *       @c x + 10 + indent is spelled in that order: the target loads 0x16
 *       and 0x22 into a register and adds @c x to it. @c name points at the
 *       ability's own name first, and @c tx / @c ty are reassigned per line;
 *       both decide which registers the locals get.
 */
static DR_AREA *drawGfLearnedWindow(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 gf, s32 ability) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u8 text[0x60];
    u8 nameBuf[0x60];
    u8 *name;
    u8 *label;
    s32 labelW;
    s32 textW;
    s32 nameW;
    s32 w;
    s32 x;
    s32 tx;
    s32 ty;
    s32 icon = ICON_ABILITY_JUNCTION;
    s32 labelIndent;
    s32 nameIndent;
    s32 col = 7;

    label = getMenuString(0x7F);
    name = getAbilityName(ability);
    copyString(nameBuf, name);
    btlStrcat2(nameBuf, getMenuString(0x7E));
    name = nameBuf;
    labelW = getFirstLineWidth(label);
    if (ability < 20) {
        icon = ICON_ABILITY_JUNCTION;
    } else if (ability < 39) {
        icon = ICON_ABILITY_JUNCTION + 1;
    } else if (ability < 58) {
        icon = ICON_ABILITY_JUNCTION + 2;
    } else if (ability < 78) {
        icon = ICON_ABILITY_JUNCTION + 3;
    } else if (ability < 83) {
        icon = ICON_ABILITY_JUNCTION + 4;
    } else if (ability < 92) {
        icon = ICON_ABILITY_JUNCTION + 5;
    } else {
        icon = ICON_ABILITY_JUNCTION + 6;
    }
    copyString(text, getMenuString(0x79));
    btlStrcat2(text, getMagicNamePtr(gf + 0x40));
    textW = getFirstLineWidth(text);
    nameW = getFirstLineWidth(name);
    nameW += 0x10;
    labelIndent = 12;
    nameIndent = 24;
    w = MAX(nameW + 0x24, MAX(textW + 0xC, labelW + 0x18));
    x = (0x16C - w) / 2;
    tx = x + 10;
    ty = y + 7;
    prim = drawDecodedText(ot, prim, tx, ty, text, col);
    tx = x + 10 + labelIndent;
    ty = y + 0x16;
    prim = drawDecodedText(ot, prim, tx, ty, label, col);
    tx = x + 10 + nameIndent;
    ty = y + 0x25;
    prim = drawIcon(ot, prim, icon, tx, ty - 2, color);
    tx += 0xE;
    prim = drawDecodedText(ot, prim, tx, ty, name, col);
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = w + 0x14;
    ctx->rect.h = 0x38;
    return drawWindow(ot, (DR_AREA *)prim, t, color);
}


/**
 * @brief Draw the "GF received N AP!" window (menu string 0x6D), centred on
 * screen.
 *
 * Formats the message into the scratchpad's fourth text buffer
 * (formatMessage), sizes a window around it (text plus 0x14 x 0xE), centres
 * that in 384 x 224, then links the text, icon 0x57 and the window, scaled
 * by @p t (drawWindow).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see scaleWindowRect).
 * @param color Colour word for the icon and window.
 * @param ap AP the GFs received.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *drawGfApWindow(P_TAG *ot, DR_AREA *prim, s32 t, s32 color, s32 ap) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    u32 size;
    u32 w;
    u32 h;
    s32 x;
    s32 y;
    DR_AREA *p;

    formatMessage(getMenuString(0x6D), ctx->text[3], 0, 0, 0, ap, 0);
    size = getTextSize(ctx->text[3]);
    h = size >> 16;
    w = size & 0xFFFF;
    w += 0x14;
    h += 0xE;
    x = (0x180 - w) >> 1;
    y = (0xE0 - h) >> 1;
    p = resetDrawArea(ot, prim);
    p = (DR_AREA *)drawDecodedText(ot, (TSPRT *)p, x + 10, y + 7, ctx->text[3], 7);
    p = drawIcon(ot, p, 0x57, x, y, color);
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = w;
    ctx->rect.h = h;
    return drawWindow(ot, p, t, color);
}


/**
 * @brief Draw one frame of the battle results screen's windows.
 *
 * Flips to the next display buffer and sets the scratchpad's origin and
 * draw-area commands from its clip rect. Then draws the title and button
 * prompt windows at progress @c titleProgress, and by @c page one more page:
 * 0 the EXP rows (drawExpPage) at @c pageProgress, 1 the item page
 * (drawItemsPage), 2 the "GF received N AP!" window (drawGfApWindow) at
 * |@c pageProgress|, 3 the GF windows @c gfWindowFlags selects at
 * |@c pageProgress|. The menu brightness and the windows' grey follow the progress
 * and are restored after each page. Stores the packet cursor back and
 * submits the frame.
 */
void drawResultsFrame(void) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    ResultsDisplay *disp;
    P_TAG *ot;
    DR_AREA *prim;
    s32 saved;
    s32 b;
    u32 c;

    flipResultsDisplay();
    PutDispEnv(&g_resultsDisplay->disp);
    disp = g_resultsDisplay;
    saved = g_menuBrightness;
    b = abs(ctx->titleProgress);
    c = (u32)b >> 5;
    ctx->originX = disp->draw.clip.x;
    ctx->originY = disp->draw.clip.y;
    ctx->drawAreaTL = 0xE3000000 | ((disp->draw.clip.y & 0x3FF) << 10) | (disp->draw.clip.x & 0x3FF);
    ctx->drawAreaBR = 0xE4000000 | (((disp->draw.clip.y + 0xDF) & 0x3FF) << 10) | ((disp->draw.clip.x + 0x17F) & 0x3FF);
    prim = disp->pktAlloc;
    ot = (P_TAG *)disp->ot;
    setMenuBrightness(b);
    c = c | (((c << 16) | 0x64000000) | (c << 8));
    prim = drawTitleWindow(ot, prim, ctx->titleProgress, c);
    prim = drawPromptWindow(ot, prim, ctx->titleProgress, c);
    setMenuBrightness(saved);
    switch (ctx->page) {
    case 0: {
        s32 t = ctx->pageProgress;
        u32 c0 = t / 32;
        if (c0 != 0) {
            s32 saved0 = g_menuBrightness;
            setMenuBrightness(t);
            prim = drawExpPage(ot, prim, c0 | (((c0 << 16) | 0x64000000) | (c0 << 8)));
            setMenuBrightness(saved0);
        }
        break;
    }
    case 1:
        prim = drawItemsPage(ot, prim);
        break;
    case 2: {
        s32 saved2 = g_menuBrightness;
        s32 b2 = abs(ctx->pageProgress);
        setMenuBrightness(b2);
        c = (u32)b2 >> 5;
        if (b2 != 0) {
            prim = drawGfApWindow(ot, prim, b2, c | (((c << 16) | 0x64000000) | (c << 8)), ctx->ap);
        }
        setMenuBrightness(saved2);
        break;
    }
    case 3: {
        s32 saved3 = g_menuBrightness;
        s32 b3 = abs(ctx->pageProgress);
        s32 flags;
        setMenuBrightness(b3);
        if (b3 != 0) {
            c = (u32)b3 >> 5;
            c = c | (((c << 16) | 0x64000000) | (c << 8));
            flags = ctx->gfWindowFlags;
            if (flags & GF_WINDOW_LEARNED) {
                prim = drawGfLearnedWindow(ot, (TSPRT *)prim, 0x58, ctx->pageProgress, c, ctx->windowGf, ctx->learnedAbility);
            }
            if (flags & GF_WINDOW_LEVEL_UP) {
                prim = drawGfLevelUpWindow(ot, (TSPRT *)prim, (flags & GF_WINDOW_LEARNED) ? 0x2D : 0x63, ctx->pageProgress, c, ctx->windowGf);
            }
        }
        setMenuBrightness(saved3);
        break;
    }
    }
    g_resultsDisplay->pktAlloc = prim;
    submitResultsDisplay();
}
