#include "common.h"
#include "battle_results/number.h"
#include "psxsdk/libgpu.h"
#include "menu_tint.h"
#include "numstr.h"

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
void *drawNumber(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color, s32 clut) {
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
        /* r, g, b and code, then u and v, then w and h are each stored as one
           word, as in the binary. */
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
 * @brief Draw a right-aligned number with drawNumber on CLUT row 7.
 *
 * @param ot OT slot the primitives are linked into.
 * @param sprt Packet cursor for the sprites.
 * @param x Packed position: the right edge x in the low half, y in the high half.
 * @param value Number to draw.
 * @param color Word stored into each sprite's r, g, b and GPU code bytes.
 * @return Packet cursor past the primitives.
 */
void *drawNumberDefault(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color) {
    return drawNumber(ot, sprt, x, value, color, 7);
}


/**
 * @brief Draw a number (drawNumber) in the menu tint.
 *
 * A @p clut of 8 or more draws in the blinking tint (g_menuTint[MENU_TINT_BLINK])
 * on CLUT row @p clut - 8; otherwise in g_menuTint[MENU_TINT_NORMAL] on row
 * @p clut.
 *
 * @param ot OT slot the primitives are linked into.
 * @param sprt Packet cursor for the sprites.
 * @param x Packed position: the right edge x in the low half, y in the high half.
 * @param value Number to draw.
 * @param clut CLUT row; 8 added selects the blinking tint.
 * @return Packet cursor past the primitives.
 */
s32 drawNumberMenuTint(s32 ot, s32 sprt, s32 x, s32 value, s32 clut) {
    s32 idx;
    if (clut >= 8) {
        clut -= 8;
        idx = MENU_TINT_BLINK;
    } else {
        idx = MENU_TINT_NORMAL;
    }
    drawNumber((P_TAG *)ot, (SPRT *)sprt, x, value, g_menuTint[idx], clut);
}


/**
 * @brief Draw a number (drawNumber) in the normal menu tint on CLUT row 7.
 *
 * @param ot OT slot the primitives are linked into.
 * @param sprt Packet cursor for the sprites.
 * @param x Packed position: the right edge x in the low half, y in the high half.
 * @param value Number to draw.
 */
void drawNumberMenuTintDefault(s32 ot, s32 sprt, s32 x, s32 value) {
    drawNumber((P_TAG *)ot, (SPRT *)sprt, x, value, g_menuTint[MENU_TINT_NORMAL], 7);
}
