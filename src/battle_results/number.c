#include "common.h"
#include "battle_results/number.h"
#include "psxsdk/libgpu.h"
#include "menu_tint.h"
#include "numstr.h"

/** One glyph of the menu font's metrics table @c D_8008371C. */
typedef struct {
    u8 width; /**< Low nibble: the glyph's advance in pixels. */
    u8 unk1;
    u16 uv; /**< u (low byte) and v (high byte) of the glyph's 12x12 texture cell. */
} FontGlyph;

extern FontGlyph D_8008371C[];
extern u8 D_80083908[];

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
