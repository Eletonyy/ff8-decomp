#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "input/button_remap.h"
#include "ui/dialog.h"
#include "ui/text.h"
#include "ui/font.h"
#include "ui/icon.h"
#include "numstr.h"
#include "game.h"
#include "menu_tint.h"

extern u8 g_digitBaseCode;
extern u16 D_80052974[];

static inline SPRT *drawSprtIcon(void *ot, s32 clut, SPRT *p, s32 idx, s32 x, s32 y, u32 color);


/**
 * @brief Get the FF8 font code for digit '0'.
 * @return Base character code for rendering digits in battle.
 */
u8 getDigitBaseCode(void) {
    return g_digitBaseCode;
}


/**
 * @brief Set the FF8 font code for digit '0'.
 * @param val Base character code from the menu string table.
 */
void setDigitBaseCode(u8 val) {
    g_digitBaseCode = val;
}


/**
 * @brief Fill the digit glyph table and the number format from menu string 11.
 *
 * The ten digit codes start at byte 1 of menu string 11; a glyph's number is
 * its character code - 0x20. For each digit, D_8008371C gets the glyph's width
 * from the font's width table, the padding that centres it in 8 pixels,
 * FONT_GLYPH_ODD in @c unk1 for an odd glyph number, and the u and v of its
 * cell in the font sheet. g_numberFormat then takes its digit codes
 * (bytes 1-16), separator and two more characters from the string.
 */
void func_8002C130(void) {
    FontGlyph *glyph;
    s32 i;
    s32 idx;
    s32 n;
    s32 v;

    glyph = D_8008371C;
    for (i = 0; i < 10; i++, glyph++) {
        idx = i + getMenuString(11)[1];
        idx -= 0x20;
        n = getNibbleValue(idx);
        glyph->width = n;
        if (n >= 9) {
            glyph->xOffset = 0;
        } else {
            glyph->xOffset = (8 - n) / 2;
        }
        glyph->unk1 = (idx & 1) ? FONT_GLYPH_ODD : 0;
        /* n is reused for u: a variable of its own takes other registers */
        n = idx % TEXT_GLYPHS_PER_ROW;
        n *= TEXT_GLYPH_SIZE;
        v = idx / TEXT_GLYPHS_PER_ROW;
        v *= TEXT_GLYPH_SIZE;
        glyph->uv = (n & 0xFF) | ((v & 0xFF) << 8);
    }
    g_numberFormat.digits[0] = getMenuString(11)[1];
    g_numberFormat.digits[1] = getMenuString(11)[2];
    g_numberFormat.digits[2] = getMenuString(11)[3];
    g_numberFormat.digits[3] = getMenuString(11)[4];
    g_numberFormat.digits[4] = getMenuString(11)[5];
    g_numberFormat.digits[5] = getMenuString(11)[6];
    g_numberFormat.digits[6] = getMenuString(11)[7];
    g_numberFormat.digits[7] = getMenuString(11)[8];
    g_numberFormat.digits[8] = getMenuString(11)[9];
    g_numberFormat.digits[9] = getMenuString(11)[10];
    g_numberFormat.digits[10] = getMenuString(11)[11];
    g_numberFormat.digits[11] = getMenuString(11)[12];
    g_numberFormat.digits[12] = getMenuString(11)[13];
    g_numberFormat.digits[13] = getMenuString(11)[14];
    g_numberFormat.digits[14] = getMenuString(11)[15];
    g_numberFormat.digits[15] = getMenuString(11)[16];
    g_numberFormat.separator = getMenuString(11)[0x14];
    g_numberFormat.unk11 = getMenuString(11)[0x11];
    g_numberFormat.unk12 = getMenuString(11)[0];
}


/**
 * @brief Load the text font: its glyph sheet and CLUT into VRAM, its widths into D_800834D8.
 *
 * The glyph sheet goes to (TEXT_FONT_X, TEXT_FONT_Y) and, if the TIM has one,
 * the CLUT to (TEXT_CLUT_X, TEXT_CLUT_Y), at most TEXT_CLUT_ROWS rows of it.
 * With @p useTimPosition set, the sheet goes where the TIM says and the CLUT is
 * not loaded. The digit glyph table is then rebuilt from the new widths
 * (func_8002C130).
 *
 * @param font The font file; nothing is loaded if it has no TIM.
 * @param useTimPosition Nonzero to load the sheet at the TIM's own position.
 */
void func_8002C3AC(NameFont *font, s32 useTimPosition) {
    u8 *p;
    u8 *dst;
    Tim *tim;
    TimSection *clut;
    TimSection *pixels;
    RECT pixRect;
    RECT clutRect;

    if (font->timOffset == 0) {
        return;
    }
    p = (u8 *)font + font->timOffset;
    tim = (Tim *)p;
    clut = &tim->clut;
    pixels = (TimSection *)((u8 *)clut + tim->clut.len);
    pixRect = pixels->rect;
    /* p is reused for the widths: a pointer of its own takes other registers */
    p = (u8 *)font;
    p += font->widthTableOffset;
    if (useTimPosition == 0) {
        pixRect.x = TEXT_FONT_X;
        pixRect.y = TEXT_FONT_Y;
        dst = D_800834D8;
        if (tim->flags & TIM_HAS_CLUT) {
            clutRect = clut->rect;
            clutRect.x = TEXT_CLUT_X;
            clutRect.y = TEXT_CLUT_Y;
            if (clutRect.h > TEXT_CLUT_ROWS) {
                clutRect.h = TEXT_CLUT_ROWS;
            }
            LoadImage(&clutRect, clut->data);
        }
    }
    /* The original's bug: dst is only set when useTimPosition is 0, so with it
     * set the widths are copied through an uninitialised pointer. */
    memcpy(dst, p, sizeof(D_800834D8));
    LoadImage(&pixRect, pixels->data);
    func_8002C130();
}


/**
 * @brief Draw one icon of @c g_iconTable as plain sprites.
 *
 * The SPRT counterpart of drawTextIcon: every cell becomes one SPRT, with no
 * texture page of its own, so the caller has to set the icons' page. Per cell:
 * the u/v/CLUT word is the cell's own plus the icons' CLUT plus @p clut; the
 * colour word is @p color with the cell's semi-transparency bit; width/height
 * are copied and the cell's signed offsets are added to (@p x, @p y).
 *
 * @note @p clut comes before @p p: the other order lets the loop optimiser
 * hoist the caller's CLUT computation out of the string loop.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param clut Added to each cell's CLUT field.
 * @param p First free packet.
 * @param idx Icon index into @c g_iconTable.
 * @param x Left edge of the icon.
 * @param y Top edge of the icon.
 * @param color Colour word of the sprites.
 * @return The first free packet after the ones written.
 */
static inline SPRT *drawSprtIcon(void *ot, s32 clut, SPRT *p, s32 idx, s32 x, s32 y, u32 color) {
    IconTable *table;
    IconCell *cell;
    u32 word;
    s32 tpage;
    u32 link;
    u32 val;
    s32 n;

    table = &g_iconTable;
    cell = (IconCell *)table; /* seeded from its own copy, see drawNextPageMarker */
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & ICON_UVCLUT_MASK;
        val += getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
        val += clut << 16;
        setIconUVClut(p, val);

        val = (word >> ICON_ABR_SHIFT) & ICON_ABR_MASK;
        val = getTPage(0, val, 0, 0);
        tpage = val;
        /* Dead code the binary requires. tpage already has the blend rate at
         * bits 5-6, so the test is never true. combine proves that only after
         * flow has kept the shift above for it, and the branch itself goes in
         * the jump pass after register allocation, so the binary has the
         * blend-rate shift (srl 30, sll 5) here with nothing using it. */
        if (tpage & ICON_ABR_MASK) {
            p->clut = tpage;
        }

        val = word >> ICON_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setlen(p, 4);
        setIconRGBC(p, val);

        word = cell->metrics;
        val = word & ICON_WH_MASK;
        setIconWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    return p;
}


/**
 * @brief Draw a string with the yellow digit icons.
 *
 * Each character of @p str is drawn 8 pixels right of the last as icon
 * (code + 0xE0) of @c g_iconTable, in plain sprites tinted with the menu tint
 * and coloured with text colour @p row's CLUT. Only codes 0x20-0x2F have an
 * icon here (the yellow digits and symbols up to ICON_STATUS_KO); other codes
 * still advance by 8 pixels, except codes below 0x19, which are skipped, and
 * 0x19-0x1F, which start a two-byte character. A texture page packet for the
 * icon sheet goes in last, so the GPU reads it before the sprites. Nothing is
 * drawn when @p y is off screen or @p str is NULL.
 *
 * @note The colour word is built one operation per statement because the
 * original computes it in its own register, see drawNextPageMarker.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param prim First free packet.
 * @param x Left edge of the first character.
 * @param y Top edge of the characters.
 * @param str Text to draw.
 * @param row Text colour, a CLUT row below TEXT_CLUT_Y.
 * @return The first free packet after the ones written.
 */
void *func_8002C56C(void *ot, void *prim, s32 x, s32 y, u8 *str, s32 row) {
    SPRT *p;
    DR_TPAGE *mode;
    u32 link;
    u32 color;
    u32 c;

    color = g_menuTint[MENU_TINT_NORMAL];
    color &= ~COLOUR_WORD_CODE;
    color |= SPRT_CODE;
    if (y > 0x100) { /* below the screen */
        return prim;
    }
    if (y < -8) { /* above the screen */
        return prim;
    }
    if (str == NULL) {
        return prim;
    }
    p = prim;
    while (1) {
        c = *str++;
        if (c == 0) {
            break;
        }
        if (c < 0x19) { /* control code */
            continue;
        }
        if (c < 0x20) { /* two-byte character */
            c -= 0x18;
            c *= 0xE0;
            c += *str++;
        }
        c += 0xE0; /* character to icon: 0x21 ('0') is ICON_YELLOW_DIGIT_0 */
        if (c < ICON_STATUS_KO) {
            /* (row << 6) + 2 moves the icons' CLUT to text colour row's
             * (TEXT_CLUT_X, TEXT_CLUT_Y + row) */
            p = drawSprtIcon(ot, (row << 6) + 2, p, c, x, y, color);
        }
        x += 8;
    }
    mode = (DR_TPAGE *)p;
    setDrawTPage(mode, 1, 0, getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y));
    addPrimFastWithTempOperand(ot, mode, link);
    return mode + 1;
}


/**
 * @brief Map a message character code to its glyph/sprite index.
 *
 * Translates a raw character byte @p c into the index used to fetch the glyph's
 * dimensions and sprite data:
 *   - @c c @c >= @c 0x40 : looked up directly in the @ref D_80052974 table
 *     (letters and the bulk of the character set), offset by @c 0x40.
 *   - @c [0x30,0x40) : digits/symbols, mapped linearly to @c c @c + @c 0x50.
 *   - @c [0x20,0x30) : button-icon codes, remapped via @c reverseButtonRemap;
 *     a valid result is biased by @c 0x80, an invalid one yields 0.
 *   - @c c @c < @c 0x20 : control codes, which have no glyph (returns 0).
 *
 * @param c Character code from a battle message string.
 * @return Glyph/sprite index, or 0 if @p c has no printable glyph.
 */
s32 func_8002C734(s32 c) {
    if (c >= 0x40) {
        c -= 0x40;
        return D_80052974[c];
    }
    if (c >= 0x20) {
        if (c >= 0x30) {
            if (c < 0x40) {
                return c + 0x50;
            }
        } else {
            c = reverseButtonRemap(c - 0x20);
            if (c >= 0) {
                return c + 0x80;
            }
            return 0;
        }
    }
    return 0;
}
