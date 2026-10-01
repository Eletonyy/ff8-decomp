#include "common.h"
#include "color.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "battle_results/display.h"
#include "btl_entity.h"
#include "btl_anim.h"
#include "dialog.h"
#include "drawbar.h"
#include "game.h"
#include "input/button_remap.h"
#include "menu_tint.h"
#include "ui/icon.h"
#include "numstr.h"

#define MAX(a, b) ((a) < (b) ? (b) : (a))

/** Bits of @c ColorRenderScratch::unk271: the windows of window group 3. */
#define MAGIC_WINDOW_SPELL 1 /**< The spell's one-line window (func_800348C4). */
#define MAGIC_WINDOW_ABILITY 2 /**< The spell, label and ability window (func_800349F4). */


/** One item reward: an item, or a card when @c id has 0x100 set (see func_8003228C). */
typedef struct {
    u16 id;
    u8 count;
    u8 pad;
} ItemReward;

/**
 * @brief Scene render context staged in PS1 scratchpad each frame.
 *
 * Populated by @c func_80034DBC during scene setup. Holds packed GPU
 * GP0 command words that @c func_8003334C emits as a DR_AREA primitive
 * once per frame, and the VRAM origin @c func_8003346C offsets its draw
 * areas by. The window drawers take their rects from here.
 */
typedef struct {
    RECT rect; /**< 0x00: window rect the drawers take. */
    RECT animRect; /**< 0x08: @c rect scaled by the open/close animation. */
    u32 drawAreaTL;  /**< 0x10: GP0(0xE3) "Drawing Area Top Left" command. */
    u32 drawAreaBR;  /**< 0x14: GP0(0xE4) "Drawing Area Bottom Right" command. */
    s16 originX; /**< 0x18: VRAM x of the buffer being drawn. */
    s16 originY; /**< 0x1A: VRAM y of the buffer being drawn. */
    s32 thread; /**< 0x1C: thread to close when the display shuts down, or 0. */
    u16 state; /**< 0x20: step of the display shutdown (func_80035158). */
    u8 pad22[0x4];
    s16 unk26; /**< 0x26: progress of the popup and description windows; negative fades. */
    s16 unk28; /**< 0x28: slide progress of the two-pane divider (see func_800337FC). */
    s16 unk2A; /**< 0x2A: progress of the item reward window; negative fades. */
    s16 unk2C; /**< 0x2C: progress of the message window (see func_8003406C). */
    s16 unk2E; /**< 0x2E: progress of the title and button prompt windows; negative fades. */
    u8 *title; /**< 0x30: message for the title window, or NULL. */
    u8 *prompt; /**< 0x34: message for the button prompt window, or NULL. */
    u8 unk38; /**< 0x38: which window group func_80034DBC adds, 0..3. */
    u8 pad39[0x3];
    u16 unk3C; /**< 0x3C: argument formatted into @c unk48. */
    u8 pad3E;
    u8 unk3F; /**< 0x3F: nonzero while the item reward line is shown. */
    u8 pad40;
    s8 timer; /**< 0x41: frames left in the shutdown's wait step. */
    u8 unk42; /**< 0x42: value formatted into the message box (see func_80034C74). */
    u8 pad43;
    u16 unk44; /**< 0x44: nonzero while the message window is shown. */
    u8 pad46[0x2];
    u8 *unk48; /**< 0x48: message for the message window. */
    u8 pad4C[0x4];
    ItemReward *reward; /**< 0x50: the item reward on display; [-1] is the previous one. */
    u8 *names[3]; /**< 0x54: name on each results row, or NULL for an empty row. */
    u8 pad60[0x80];
    u8 text[4][0x18]; /**< 0xE0: string buffers for the drawers. */
    u8 pad140[0xE8];
    u32 unk228[3]; /**< 0x228: third number on each results row. */
    u32 unk234[3]; /**< 0x234: second number on each results row. */
    u32 unk240[3]; /**< 0x240: first number on each results row. */
    u8 pad24C[0x23];
    u8 unk26F; /**< 0x26F: ability id for func_800349F4's window. */
    u8 unk270; /**< 0x270: magic id for the magic windows. */
    u8 unk271; /**< 0x271: MAGIC_WINDOW_* bits, the windows to draw. */
    u8 unk272[3]; /**< 0x272: number after icon 0xE on each results row. */
    u8 unk275[3]; /**< 0x275: progress of each row's menu string 0x30 popup; 0 hides it. */
    u8 nameColor[3]; /**< 0x278: text colour of each row's name. */
    u8 unk27B; /**< 0x27B: width of the menu string 0x30 popup. */
    u8 unk27C[3]; /**< 0x27C: progress of each row's centred menu string 0x31 popup, 1..0x40. */
    u8 unk27F; /**< 0x27F: text width of the menu string 0x31 popup. */
} ColorRenderScratch;

/* --- Private functions --- */

static DR_AREA *func_8003346C(P_TAG *ot, DR_AREA *prim, RECT *rect);
static DR_AREA *func_800335AC(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *func_80033688(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static s32 func_80033768(s32 x, s32 w, s32 dir, s32 t);
static DR_AREA *func_800337FC(P_TAG *ot, DR_AREA *prim, u8 *fromMsg, u8 *toMsg, s32 color);
static DR_AREA *func_80033A28(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *func_80033C7C(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *func_80033D5C(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *func_80033F1C(P_TAG *ot, DR_AREA *prim, s32 t, s32 color);
static DR_AREA *func_8003406C(P_TAG *ot, DR_AREA *prim, s32 color);
static DR_AREA *func_800341BC(P_TAG *ot, DR_AREA *prim);
static DR_AREA *func_8003431C(P_TAG *ot, DR_AREA *prim, s32 idx, s32 x, s32 y, s32 t, s32 color);
static DR_AREA *func_80034830(P_TAG *ot, DR_AREA *prim, s32 color);
static DR_AREA *func_800348C4(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 magic);
static DR_AREA *func_800349F4(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 magic, s32 ability);
static DR_AREA *func_80034C74(P_TAG *ot, DR_AREA *prim, s32 t, s32 color, s32 value);
static void func_80034DBC(void);

/**
 * @brief Draw @p value as a right-aligned row of 12x12 digit sprites.
 *
 * Formats @p value into the digit buffer @c D_80083908 with @c intToDecString
 * (digit base 1, so digit d is stored as d + 1), skips up to nine leading zeros
 * so at least one digit is left, and sums the glyph advances from the font
 * metrics table @c D_8008371C to right-align the number at x. Each digit becomes
 * one SPRT linked into @p ot, then a DR_TPAGE selects the font's texture page.
 *
 * @param ot OT slot the primitives are linked into.
 * @param sprt Packet cursor for the sprites.
 * @param x Packed position: the right edge x in the low half, y in the high half.
 * @param value Number to draw.
 * @param color Word stored into each sprite's r, g, b and GPU code bytes.
 * @param clut CLUT row, counted down from (288, 224).
 * @return Packet cursor past the DR_TPAGE.
 */
void *func_800330F4(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color, s32 clut) {
    FontGlyph *table;
    u8 *digits;
    u8 *p;
    s32 y;
    s32 i;
    s32 width;
    u32 link1;
    u32 link2;

    table = D_8008371C;
    table--;
    /* Dead, since digits is set again after the call, but the binary needs it:
       gcc drops the branch only in its last jump pass, after it has shaped
       register allocation and scheduling. */
    if (value == 0) {
        digits = &D_80083908[9];
    }
    y = x >> 16;
    x <<= 16;
    x >>= 16;
    intToDecString(value, D_80083908, 1);
    digits = D_80083908;
    for (i = 0; i < 9; i++) {
        if (*digits != 1) {
            break;
        }
        digits++;
    }

    width = 0;
    p = digits;
    while (1) {
        s32 c = *p++;
        if (c == 0) {
            break;
        }
        width += table[c].width & 0xF;
    }
    x -= width;

    p = digits;
    while (1) {
        s32 c = *p++;
        FontGlyph *g;
        u16 uv;
        if (c == 0) {
            break;
        }
        g = &table[c];
        uv = g->uv;
        *(u32 *)&sprt->r0 = color;
        setlen(sprt, 4);
        sprt->x0 = x;
        sprt->y0 = y;
        sprt->clut = getClut(288, 224) + (clut << 6);
        *(u16 *)&sprt->u0 = uv;
        *(u32 *)&sprt->w = 0xC000C;
        addPrimFastWithTempOperand(ot, sprt, link1);
        sprt++;
        x += g->width & 0xF;
    }

    setlen(sprt, 1);
    ((DR_TPAGE *)sprt)->code[0] = _get_mode(1, 0, getTPage(0, 0, 960, 256));
    addPrimFastWithTempOperand(ot, sprt, link2);
    return (DR_TPAGE *)sprt + 1;
}


/**
 * @brief Draw a right-aligned number with func_800330F4 on CLUT row 7.
 *
 * @param ot OT slot the primitives are linked into.
 * @param sprt Packet cursor for the sprites.
 * @param x Packed position: the right edge x in the low half, y in the high half.
 * @param value Number to draw.
 * @param color Word stored into each sprite's r, g, b and GPU code bytes.
 * @return Packet cursor past the primitives.
 */
void *drawColorDefault(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color) {
    return func_800330F4(ot, sprt, x, value, color, 7);
}


/**
 * @brief Call func_800330F4 with a color from g_menuTint selected by arg4.
 *
 * If arg4 >= 8, subtracts 8 and uses g_menuTint[MENU_TINT_BLINK]; otherwise uses
 * g_menuTint[MENU_TINT_NORMAL]. Passes the selected color as the 5th arg and the
 * modified arg4 as the 6th.
 *
 * @param a0 First argument passed through.
 * @param a1 Second argument passed through.
 * @param a2 Third argument passed through.
 * @param a3 Fourth argument passed through.
 * @param arg4 Mode index; values >= 8 select the alternate color table.
 */
s32 drawColorByMenuPalette(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg4) {
    s32 idx;
    if (arg4 >= 8) {
        arg4 -= 8;
        idx = MENU_TINT_BLINK;
    } else {
        idx = MENU_TINT_NORMAL;
    }
    func_800330F4((P_TAG *)a0, (SPRT *)a1, a2, a3, g_menuTint[idx], arg4);
}


/**
 * Calls func_800330F4 with g_menuTint[MENU_TINT_NORMAL] as the 5th arg and 7 as the 6th (mode).
 *
 * @param a0 First argument passed through
 * @param a1 Second argument passed through
 * @param a2 Third argument passed through
 * @param a3 Fourth argument passed through
 */
void drawMenuColorDefault(s32 a0, s32 a1, s32 a2, s32 a3) {
    func_800330F4((P_TAG *)a0, (SPRT *)a1, a2, a3, g_menuTint[MENU_TINT_NORMAL], 7);
}


/**
 * @brief Emit a @c DR_AREA primitive into the OT and advance the packet.
 *
 * Loads the current frame's drawing-area GP0 commands from the scratchpad
 * @c ColorRenderScratch staged by @c func_80034DBC, packs them into the
 * @c DR_AREA primitive at @c prim, links @c prim into @c ot's slot chain
 * via @c addPrimFast (temp $a3), and returns the next packet cursor.
 *
 * @param ot   OT slot pointer.
 * @param prim Storage for the new primitive (must have space for one DR_AREA).
 * @return Cursor for the next primitive (@c prim @c + @c 1).
 */
DR_AREA *func_8003334C(P_TAG *ot, DR_AREA *prim) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
void func_80033380(s32 t, RECT *src, RECT *dst) {
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
 * @c ColorRenderScratch, keeps it at least 2x2, packs it into @p prim with
 * @c SetDrawArea and links @p prim into @p ot via @c addPrimFast (temp $s1).
 *
 * @param ot OT slot pointer.
 * @param prim Storage for the new primitive.
 * @param rect Draw area, relative to the buffer's origin.
 * @return Cursor for the next primitive (@c prim @c + @c 1).
 */
static DR_AREA *func_8003346C(P_TAG *ot, DR_AREA *prim, RECT *rect) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
 * @brief Inset a rectangle by 1 pixel on each side and dispatch for rendering.
 *
 * Saves the original rectangle coordinates (4 u16 values), adjusts them
 * (x+1, y+1, w-2, h-2), calls func_8003346C to render with the inset rect,
 * then restores the original values.
 *
 * @param ot OT slot pointer, passed to func_8003346C.
 * @param prim Storage for the primitive, passed to func_8003346C.
 * @param rect Rectangle to inset (modified temporarily).
 * @return Cursor for the next primitive.
 */
DR_AREA *drawInsetRect(P_TAG *ot, DR_AREA *prim, RECT *rect) {
    s32 save0 = *(s32 *)&rect->x;
    s32 save1 = *(s32 *)&rect->w;
    rect->x += 1;
    rect->y += 1;
    rect->w -= 2;
    rect->h -= 2;
    prim = func_8003346C(ot, prim, rect);
    *(s32 *)&rect->x = save0;
    *(s32 *)&rect->w = save1;
    return prim;
}


/**
 * @brief Draw the scratchpad window, scaled by its open/close animation.
 *
 * Scales @c rect by |@p t| into @c animRect (func_80033380), then links the
 * frame (func_8002B898) on the scaled rect and the background (func_8002B8BC)
 * on the full one. Mid-animation (|@p t| < 0x1000) it brackets them with draw
 * areas on the scaled rect, the first one inset by a pixel. Draws nothing when
 * @p t is 0.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Animation progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the frame and background.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_800335AC(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 a;

    a = abs(t);
    if (a != 0) {
        func_80033380(a, &ctx->rect, &ctx->animRect);
        if (a < 0x1000) {
            prim = drawInsetRect(ot, prim, &ctx->animRect);
        }
        prim = func_8002B898(ot, prim, &ctx->animRect, color);
        prim = func_8002B8BC(ot, prim, &ctx->rect, color, 0);
        if (a < 0x1000) {
            prim = func_8003346C(ot, prim, &ctx->animRect);
        }
    }
    return prim;
}


/**
 * @brief func_800335AC, passing 8 instead of 0 as func_8002B8BC's last argument.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Animation progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the frame and background.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80033688(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 a;

    a = abs(t);
    if (a != 0) {
        func_80033380(a, &ctx->rect, &ctx->animRect);
        if (a < 0x1000) {
            prim = drawInsetRect(ot, prim, &ctx->animRect);
        }
        prim = func_8002B898(ot, prim, &ctx->animRect, color);
        prim = func_8002B8BC(ot, prim, &ctx->rect, color, 8);
        if (a < 0x1000) {
            prim = func_8003346C(ot, prim, &ctx->animRect);
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
static s32 func_80033768(s32 x, s32 w, s32 dir, s32 t) {
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
 * As @c unk28 grows, an 8-pixel divider slides right across the scratchpad
 * window rect along g_animCurveFadeIn (func_80033768). The pane left of it
 * shows @p toMsg moving in with the divider; the pane right of it keeps
 * @p fromMsg in place. Each pane is clipped to its part of the window
 * (drawInsetRect) and skipped once it has no width; text sits 9 pixels below
 * the rect's top. The divider is drawn last with func_8002B3A0 in @p color.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param fromMsg Message being wiped out, or NULL.
 * @param toMsg Message being wiped in, or NULL.
 * @param color Colour word for the divider.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_800337FC(P_TAG *ot, DR_AREA *prim, u8 *fromMsg, u8 *toMsg, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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

    prim = func_8003334C(ot, prim);
    split = func_80033768(ctx->rect.x, ctx->rect.w, 0, ctx->unk28);
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
        prim = drawInsetRect(ot, prim, &ctx->animRect);
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
        prim = drawInsetRect(ot, prim, &ctx->animRect);
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
 * then, while @c unk3F is set, the current reward's name and count: wiped in
 * from the previous reward's name (func_800337FC) while @c unk28 runs, or
 * plain otherwise. The window itself is drawn last, scaled by @p t
 * (func_80033688).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the frame, text and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80033A28(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 slide;
    s32 x;
    s32 y;

    if (t == 0) {
        return prim;
    }
    prim = func_8003334C(ot, prim);
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
    slide = ctx->unk28;
    if (ctx->unk3F != 0) {
        if (slide != 0) {
            ctx->rect.x = x;
            ctx->rect.y = y;
            ctx->rect.w = 0xBD;
            ctx->rect.h = 0x1A;
            func_8003228C(ctx->reward[-1].id, ctx->text[0]);
            func_8003228C(ctx->reward[0].id, ctx->text[1]);
            prim = func_800337FC(ot, prim, ctx->text[0], ctx->text[1], color);
            prim = drawColorDefault(ot, (SPRT *)prim, ((y + 9) << 16) | (x + 0xE6), ctx->reward[-1].count, color);
        } else {
            func_8003228C(ctx->reward[0].id, ctx->text[2]);
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 10, y + 9, ctx->text[2], 7);
            prim = drawColorDefault(ot, (SPRT *)prim, ((y + 9) << 16) | (x + 0xE6), ctx->reward[0].count, color);
        }
    }
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = 0xF0;
    ctx->rect.h = 0x1A;
    return func_80033688(ot, prim, t, color);
}


/**
 * @brief Draw the title window: icon 0x57 and the @c title message.
 *
 * Draws nothing when @p t is 0. The window (0x10, 8, 0x160 x 0x1A) is drawn
 * last, scaled by @p t (func_800335AC).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the icon and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80033C7C(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 y;

    if (t == 0) {
        return prim;
    }
    prim = func_8003334C(ot, prim);
    y = 8;
    prim = drawIcon(ot, prim, 0x57, 0x10, y, color);
    if (ctx->title != NULL) {
        prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, 0x1A, 0x11, ctx->title, 7);
    }
    ctx->rect.x = 0x10;
    ctx->rect.y = y;
    ctx->rect.w = 0x160;
    ctx->rect.h = 0x1A;
    return func_800335AC(ot, prim, t, color);
}


/**
 * @brief Draw the description window for the item reward on display.
 *
 * Draws nothing when @p t is 0. Otherwise links icon 0x55 and, while
 * @c unk3F is set, the reward's description (guardedEntityLookup; cards have
 * none): wiped in from the previous reward's (func_800337FC) while @c unk28
 * runs, or decoded and drawn plain otherwise. The window (0x10, 0x9C,
 * 0x160 x 0x1A) is drawn last, scaled by @p t (func_800335AC).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the icon, text and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80033D5C(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
    prim = func_8003334C(ot, prim);
    x = 0x10;
    y = 0x9C;
    prim = drawIcon(ot, prim, 0x55, x, y, color);
    slide = ctx->unk28;
    if (ctx->unk3F != 0) {
        if (slide != 0) {
            ctx->rect.x = x;
            ctx->rect.y = y;
            ctx->rect.w = 0x160;
            ctx->rect.h = 0x1A;
            from = (u8 *)guardedEntityLookup(ctx->reward[-1].id);
            to = (u8 *)guardedEntityLookup(ctx->reward[0].id);
            if (from != to || from != NULL) {
                prim = func_800337FC(ot, prim, from, to, color);
            }
        } else {
            msg = (u8 *)guardedEntityLookup(ctx->reward[0].id);
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
    return func_800335AC(ot, prim, t, color);
}


/**
 * @brief Draw the button prompt window: a button icon and the @c prompt text.
 *
 * Draws nothing when @p t is 0. The icon (reverseButtonRemap(6) + 0x80) and
 * the first line of @c prompt are centred together along y = 0xC0. The window
 * (0x10, 0xB8, 0x160 x 0x1A) is drawn last, scaled by @p t (func_800335AC),
 * and the full draw area is restored after it.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the icon and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80033F1C(P_TAG *ot, DR_AREA *prim, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    u8 *msg;
    s32 x;
    s32 icon;

    if (t == 0) {
        return prim;
    }
    prim = func_8003334C(ot, prim);
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
    prim = func_800335AC(ot, prim, t, color);
    return func_8003334C(ot, prim);
}


/**
 * @brief Draw the message window: message @c unk48 in a box centred on screen.
 *
 * Only while @c unk44 is set and the window's progress @c unk2C is nonzero.
 * Formats @c unk48 with @c unk3C into the scratchpad's fourth text buffer
 * (func_80032350), sizes a window around it (text plus 0x14 x 0xE), centres
 * it on x 0xC0, y 0x7E, then links icon 0x56, the text and the window,
 * scaled by @c unk2C (func_80033688).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param color Colour word for the icon and window.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_8003406C(P_TAG *ot, DR_AREA *prim, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
    if (ctx->unk2C == 0) {
        return prim;
    }
    func_80032350(ctx->unk48, ctx->text[3], ctx->unk3C, 0, 0, 0, 0);
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
    t = ctx->unk2C;
    prim = drawIcon(ot, prim, 0x56, x, y, color);
    prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 10, y + 7, ctx->text[3], 7);
    return func_80033688(ot, prim, t, color);
}


/**
 * @brief Draw the popup, description and item reward windows at their progress.
 *
 * The popup and description windows run on @c unk26, the reward window on
 * @c unk2A. A negative progress fades: the menu brightness and the windows'
 * grey colour follow |progress|; otherwise both are at full (0x1000, 0x808080).
 * A window group at progress 0 is not drawn.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_800341BC(P_TAG *ot, DR_AREA *prim) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 t;
    s32 b;
    u32 color;

    t = ctx->unk26;
    if (t < 0) {
        b = abs(t);
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
        prim = func_8003406C(ot, prim, color);
        prim = func_80033D5C(ot, prim, t, color);
    }
    t = ctx->unk2A;
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
        prim = func_80033A28(ot, prim, t, color);
    }
    return prim;
}


/**
 * @brief Draw one character row of the battle results screen.
 *
 * Only a row with a name: first its menu string 0x30 popup while @c unk275
 * counts (progress @c unk275 << 9), and the centred menu string 0x31 popup
 * while @c unk27C is 1..0x40 (progress << 6). Then the name in its colour,
 * the labels (menu strings 0x1D..0x1F), the row's three numbers as 9-digit
 * strings with icon 0xC after each, icon 0xE and the @c unk272 number. The
 * row's window is drawn last, scaled by @p t.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param idx Row (character) index, 0..2.
 * @param x Left of the row.
 * @param y Top of the row.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the icons and windows.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_8003431C(P_TAG *ot, DR_AREA *prim, s32 idx, s32 x, s32 y, s32 t, s32 color) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
        if (ctx->unk275[idx] != 0) {
            prim = func_8003334C(ot, prim);
            col = 7;
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, x + 0x5D, y + 8, getMenuString(0x30), col);
            ctx->rect.x = x + 0x53;
            ctx->rect.w = ctx->unk27B;
            ctx->rect.y = y + 4;
            ctx->rect.h = 0x14;
            progress = ctx->unk275[idx] << 9;
            prim = func_800335AC(ot, prim, progress < 0 ? 0 : progress > 0x1000 ? 0x1000 : progress, color);
        }
        popup = ctx->unk27C[idx];
        if (popup != 0 && popup < 0x41) {
            prim = func_8003334C(ot, prim);
            col = 7;
            str = getMenuString(0x31);
            tx = (0x194 - ctx->unk27F) / 2;
            prim = (DR_AREA *)drawDecodedText(ot, (TSPRT *)prim, tx, y + 8, str, col);
            ctx->rect.x = tx - 10;
            ctx->rect.y = y + 4;
            ctx->rect.h = 0x14;
            ctx->rect.w = ctx->unk27F + 0x14;
            progress = ctx->unk27C[idx] << 6;
            prim = func_800335AC(ot, prim, progress < 0 ? 0 : progress > 0x1000 ? 0x1000 : progress, color);
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
        intToDecString(ctx->unk240[idx], str, digit);
        replaceLeadingZeros(str, 9, digit, blank);
        prim = func_8002C56C(ot, prim, tx, ty, str, col);
        tx += 0x50;
        prim = drawIcon(ot, prim, 0xC, tx, ty, color);
        tx = x + 0xE3;
        ty = y + 0x16;
        intToDecString(ctx->unk234[idx], str, digit);
        replaceLeadingZeros(str, 9, digit, blank);
        prim = func_8002C56C(ot, prim, tx, ty, str, col);
        tx += 0x50;
        prim = drawIcon(ot, prim, 0xC, tx, ty, color);
        tx = x + 0xE3;
        ty = y + 0x25;
        intToDecString(ctx->unk228[idx], str, digit);
        replaceLeadingZeros(str, 9, digit, blank);
        prim = func_8002C56C(ot, prim, tx, ty, str, col);
        tx += 0x50;
        prim = drawIcon(ot, prim, 0xC, tx, ty, color);
        tx = x + 10;
        ty = y + 0x12;
        prim = drawIconClut(ot, (TSPRT *)prim, 0xE, tx, ty, color, 0x1C2);
        tx = x + 0x3E;
        ty = y + 0x14;
        prim = func_800330F4(ot, (SPRT *)prim, (ty << 16) | (tx & 0xFFFF), ctx->unk272[idx], color, col);
    }
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = 0x143;
    ctx->rect.h = 0x32;
    return func_800335AC(ot, prim, t, color);
}


/**
 * @brief Render three rows of stat delta bars.
 *
 * Calls func_8003431C three times with incrementing row index (0, 1, 2)
 * and a y-offset that advances by 0x32 each row. Returns the result of
 * the last call.
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor, threaded through each call.
 * @param color Colour word passed to each row.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80034830(P_TAG *ot, DR_AREA *prim, s32 color) {
    s32 rowWidth = 0x1E;
    s32 yOffset = 0x22;
    s32 i = 0;
    s32 scale = 0x1000;
    do {
        prim = func_8003431C(ot, prim, i, rowWidth, yOffset, scale, color);
        i++;
        yOffset += 0x32;
    } while (i < 3);
    return prim;
}


/**
 * @brief Draw a one-line window naming a magic spell, centred at row @p y.
 *
 * The text is menu string 0x79, the name of magic @p magic + 0x40, a blank
 * (the first character of menu string 0xB) and menu string 0x30. The window
 * fits the text's first line plus 8 pixels on each side and is drawn scaled
 * by @p t (func_800335AC).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor; the text's sprites come first.
 * @param y Top of the window.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the window.
 * @param magic Magic id, offset by 0x40 into the name table.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_800348C4(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 magic) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    u8 text[0x80];
    u8 sep[2];
    s32 w;
    s32 x;

    sep[0] = getMenuString(0xB)[0];
    sep[1] = 0;
    copyString(text, getMenuString(0x79));
    btlStrcat2(text, getMagicNamePtr(magic + 0x40));
    btlStrcat2(text, sep);
    btlStrcat2(text, getMenuString(0x30));
    w = getFirstLineWidth(text);
    x = (0x170 - w) / 2;
    prim = drawDecodedText(ot, prim, x + 8, y + 7, text, 7);
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = w + 0x10;
    ctx->rect.h = 0x1A;
    return func_800335AC(ot, (DR_AREA *)prim, t, color);
}


/**
 * @brief Draw a three-line window: a magic spell, a label and an ability with its icon.
 *
 * The first line is menu string 0x79 and the name of magic @p magic + 0x40.
 * The second is menu string 0x7F, indented by 12. The third is the icon of
 * the ability's category (the ranges of getAbilityCategory) and the ability's
 * name followed by menu string 0x7E, indented by 24. The window fits the
 * widest line, is centred in 384 and is drawn scaled by @p t (func_800335AC).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor; the text and icon sprites come first.
 * @param y Top of the window.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the icon and window.
 * @param magic Magic id, offset by 0x40 into the name table.
 * @param ability Ability id.
 * @return Packet cursor after the primitives.
 * @note The two indents are variables set before the width is computed and
 *       @c x + 10 + indent is spelled in that order: the target loads 0x16
 *       and 0x22 into a register and adds @c x to it. @c name points at the
 *       ability's own name first, and @c tx / @c ty are reassigned per line;
 *       both decide which registers the locals get.
 */
static DR_AREA *func_800349F4(P_TAG *ot, TSPRT *prim, s32 y, s32 t, s32 color, s32 magic, s32 ability) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
    btlStrcat2(text, getMagicNamePtr(magic + 0x40));
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
    return func_800335AC(ot, (DR_AREA *)prim, t, color);
}


/**
 * @brief Draw a message box centred on screen: menu string 0x6D with @p value.
 *
 * Formats the message into the scratchpad's fourth text buffer
 * (func_80032350), sizes a window around it (text plus 0x14 x 0xE), centres
 * that in 384 x 224, then links the text, icon 0x57 and the window, scaled
 * by @p t (func_800335AC).
 *
 * @param ot OT slot pointer.
 * @param prim Packet cursor.
 * @param t Window open/close progress, -0x1000..0x1000 (see func_80033380).
 * @param color Colour word for the icon and window.
 * @param value Value formatted into the message.
 * @return Packet cursor after the primitives.
 */
static DR_AREA *func_80034C74(P_TAG *ot, DR_AREA *prim, s32 t, s32 color, s32 value) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    u32 size;
    u32 w;
    u32 h;
    s32 x;
    s32 y;
    DR_AREA *p;

    func_80032350(getMenuString(0x6D), ctx->text[3], 0, 0, 0, value, 0);
    size = getTextSize(ctx->text[3]);
    h = size >> 16;
    w = size & 0xFFFF;
    w += 0x14;
    h += 0xE;
    x = (0x180 - w) >> 1;
    y = (0xE0 - h) >> 1;
    p = func_8003334C(ot, prim);
    p = (DR_AREA *)drawDecodedText(ot, (TSPRT *)p, x + 10, y + 7, ctx->text[3], 7);
    p = drawIcon(ot, p, 0x57, x, y, color);
    ctx->rect.x = x;
    ctx->rect.y = y;
    ctx->rect.w = w;
    ctx->rect.h = h;
    return func_800335AC(ot, p, t, color);
}


/**
 * @brief Draw one frame of the battle results screen's windows.
 *
 * Flips to the next display buffer and sets the scratchpad's origin and
 * draw-area commands from its clip rect. Then draws the title and button
 * prompt windows at progress @c unk2E, and by @c unk38 one more group:
 * 0 func_80034830's rows at @c unk2A, 1 the popup, description and item
 * reward windows (func_800341BC), 2 the message box with @c unk42
 * (func_80034C74) at |@c unk2A|, 3 the magic windows @c unk271 selects at
 * |@c unk2A|. The menu brightness and the windows' grey follow the progress
 * and are restored after each group. Stores the packet cursor back and
 * submits the frame.
 */
static void func_80034DBC(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
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
    b = abs(ctx->unk2E);
    c = (u32)b >> 5;
    ctx->originX = disp->draw.clip.x;
    ctx->originY = disp->draw.clip.y;
    ctx->drawAreaTL = 0xE3000000 | ((disp->draw.clip.y & 0x3FF) << 10) | (disp->draw.clip.x & 0x3FF);
    ctx->drawAreaBR = 0xE4000000 | (((disp->draw.clip.y + 0xDF) & 0x3FF) << 10) | ((disp->draw.clip.x + 0x17F) & 0x3FF);
    prim = disp->pktAlloc;
    ot = (P_TAG *)disp->ot;
    setMenuBrightness(b);
    c = c | (((c << 16) | 0x64000000) | (c << 8));
    prim = func_80033C7C(ot, prim, ctx->unk2E, c);
    prim = func_80033F1C(ot, prim, ctx->unk2E, c);
    setMenuBrightness(saved);
    switch (ctx->unk38) {
    case 0: {
        s32 t = ctx->unk2A;
        u32 c0 = t / 32;
        if (c0 != 0) {
            s32 saved0 = g_menuBrightness;
            setMenuBrightness(t);
            prim = func_80034830(ot, prim, c0 | (((c0 << 16) | 0x64000000) | (c0 << 8)));
            setMenuBrightness(saved0);
        }
        break;
    }
    case 1:
        prim = func_800341BC(ot, prim);
        break;
    case 2: {
        s32 saved2 = g_menuBrightness;
        s32 b2 = abs(ctx->unk2A);
        setMenuBrightness(b2);
        c = (u32)b2 >> 5;
        if (b2 != 0) {
            prim = func_80034C74(ot, prim, b2, c | (((c << 16) | 0x64000000) | (c << 8)), ctx->unk42);
        }
        setMenuBrightness(saved2);
        break;
    }
    case 3: {
        s32 saved3 = g_menuBrightness;
        s32 b3 = abs(ctx->unk2A);
        s32 flags;
        setMenuBrightness(b3);
        if (b3 != 0) {
            c = (u32)b3 >> 5;
            c = c | (((c << 16) | 0x64000000) | (c << 8));
            flags = ctx->unk271;
            if (flags & MAGIC_WINDOW_ABILITY) {
                prim = func_800349F4(ot, (TSPRT *)prim, 0x58, ctx->unk2A, c, ctx->unk270, ctx->unk26F);
            }
            if (flags & MAGIC_WINDOW_SPELL) {
                prim = func_800348C4(ot, (TSPRT *)prim, (flags & MAGIC_WINDOW_ABILITY) ? 0x2D : 0x63, ctx->unk2A, c, ctx->unk270);
            }
        }
        setMenuBrightness(saved3);
        break;
    }
    }
    g_resultsDisplay->pktAlloc = prim;
    submitResultsDisplay();
}


/**
 * @brief Enable the display and execute a full rendering pass.
 *
 * Calls SetDispMask(1) to make the framebuffer visible, then calls
 * func_80034DBC (main rendering) followed by func_8003283C (scene submission).
 */
void enableDisplayAndRender(void) {
    SetDispMask(1);
    func_80034DBC();
    func_8003283C();
}


extern u8 D_80083928;
extern u8 D_80083929;
/**
 * @brief Get the current value of the global flag D_80083928.
 * @return The flag value as an unsigned byte.
 */
u8 getRenderCompleteFlag(void) {
    return D_80083928;
}


/**
 * @brief Step the display shutdown, one step per call.
 *
 * Steps through @c state: blank the display, clear the results display
 * twice (the second time with func_8003283C), render until @c D_80083929
 * reads 1, wait two frames on @c timer, close @c thread if there is one
 * (setting @c D_80083928), then keep the display blanked.
 */
void func_80035158(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    u16 *state = &ctx->state;
    s32 thread;

    switch (*state) {
    case 0:
        SetDispMask(0);
        *state = 1;
        break;
    case 1:
        clearResultsDisplay();
        *state = 2;
        break;
    case 2:
        clearResultsDisplay();
        func_8003283C();
        *state = 3;
        break;
    case 3:
        enableDisplayAndRender();
        if (D_80083929 == 1) {
            *state = 4;
        }
        break;
    case 4:
        ctx->timer = 2;
        *state = 5;
        break;
    case 5:
        if (--ctx->timer <= 0) {
            *state = 6;
        }
        break;
    case 6:
        thread = ctx->thread;
        if (thread != 0) {
            ctx->thread = 0;
            closeThreadSafe(thread);
            D_80083928 = 1;
            SetDispMask(0);
        }
        *state = 7;
        break;
    case 7:
        SetDispMask(0);
        break;
    }
}


