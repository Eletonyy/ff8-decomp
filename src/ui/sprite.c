#include "common.h"
#include "ui/sprite.h"
#include "gamestate.h"

extern u8 *getCharName(CharacterId charId);
extern SpriteTable g_portraitsTable;

#define STRIP_STRIDE   0x300
#define STRIP_ROWS     12
#define STRIP_PITCH    0x30

#define SCRATCH_PITCH  0x60
#define SCRATCH_MARGIN 0x0C
#define SCRATCH_USED   (SCRATCH_MARGIN + STRIP_ROWS * SCRATCH_PITCH)

#define GLYPHS_PER_ROW 21
#define GLYPH_WIDTH    6
#define GLYPH_HEIGHT   12
#define SHEET_PITCH    512

#define setRGBCWord(p, c) (*(u32 *)&(p)->r0 = (c))
#define setUVClutWH(p, uv, wh) (*(u32 *)&(p)->u0 = (uv), *(u32 *)&(p)->w = (wh))

#define setSpriteCellXYTpage(p, xyTpage, x, y, color) do { \
    s32 dx_, dy_; \
    dy_ = (s8)((xyTpage) >> 8); \
    dx_ = (s8)(xyTpage); \
    dx_ += (x); \
    dy_ += (y); \
    setXY0(p, dx_, dy_); \
    (xyTpage) >>= 16; \
    setRGBCWord(p, color); \
    setTSprtFull(p, 0, 0, xyTpage); \
} while (0)

#define setSpriteCell(p, cell, x, y, color) { \
    u32 uv_ = (cell)->uvClut; \
    u32 wh_ = (cell)->wh; \
    u32 xy_ = (cell)->xyTpage; \
    setUVClutWH(p, uv_, wh_); \
    setSpriteCellXYTpage(p, xy_, x, y, color); \
}

void func_80037308(NameFont *font, u8 *out)
{
    u8 scratch[0x490];
    u8 *widths;
    u8 *pixels;
    u8 *dstSlot;
    u8 *pen;
    u8 *scratchPtr;
    u8 *clear;
    u8 *name;
    u8 *src;
    u8 *dst;
    u8 *packSrc;
    CharacterData *ch;
    s32 penX;
    s32 slot;
    s32 width;
    s32 rowOffset;
    s32 colOffset;
    s32 rows;
    s32 count;
    s32 row;
    s32 col;
    u32 code;
    s32 charId;
    u8 b;
    u8 hi;
    u8 lo;

    dstSlot = out;
    clear = dstSlot;

    for (slot = PARTY_SLOT_COUNT * STRIP_STRIDE - 1; slot >= 0; slot--) {
        *clear++ = 0;
    }

    do {
        for (slot = 0; slot < PARTY_SLOT_COUNT; slot++, dstSlot += STRIP_STRIDE) {
            scratchPtr = scratch;
            pen = scratch + SCRATCH_MARGIN;

            charId = g_gameState.mainData.party.partyMembers[slot];

            if (charId == PARTY_SLOT_EMPTY) {
                continue;
            }

            penX = 0;

            pixels = (u8 *)font;
            widths = pixels + ((NameFont *)pixels)->widthTableOffset;
            pixels += ((NameFont *)pixels)->timOffset;
            pixels += 8;
            pixels += ((TimSection *)pixels)->len;
            pixels += 0xC;

            for (col = SCRATCH_USED - 1; col >= 0; col--) {
                *scratchPtr++ = 0;
            }

            ch = g_gameState.chars;
            ch += charId;
            name = getCharName(ch->characterId);

            while (1) {
                code = *name++;

                if (code == 0) {
                    break;
                }

                if (code < 0x20) {
                    code -= 0x19;
                    code = code * 224;
                    code += *name++;
                }

                code -= 0x20;

                width = widths[code >> 1];

                if (code & 1) {
                    width >>= 4;
                }

                width &= 0xF;

                if (width != 0) {
                    width--;
                }

                dst = pen;
                rows = STRIP_ROWS;

                row = code / GLYPHS_PER_ROW;
                col = code % GLYPHS_PER_ROW;

                code = row * GLYPH_HEIGHT * (SHEET_PITCH / 4);
                colOffset = col * GLYPH_WIDTH;
                src = (pixels + colOffset) + code;

                for (; rows > 0; rows--) {
                    for (col = GLYPH_WIDTH; col > 0; col--) {
                        b = *src++;

                        *dst++ |= b & 0xF;
                        *dst++ |= b >> 4;
                    }

                    dst += SCRATCH_PITCH - GLYPH_WIDTH * 2;
                    src += (SHEET_PITCH / 4) - GLYPH_WIDTH;
                }

                pen += width;
                penX += width;
            }

            pixels = dstSlot + STRIP_PITCH;
            pen++;
            penX = (penX + 2) / 2;

            for (rows = STRIP_ROWS; rows > 0; rows--) {
                packSrc = pen;
                count = penX;
                dst = pixels;

                while (count > 0) {
                    hi = *--packSrc;
                    lo = *--packSrc;
                    *--dst = (lo & 0xF) | (hi << 4);
                    count--;
                }

                pen += SCRATCH_PITCH;
                pixels += STRIP_PITCH;
            }
        }
    } while (0);
}

/**
 * @brief Draw portrait sprite @p idx of the portrait table at a screen position.
 *
 * @param ot Ordering-table entry the packets are linked into.
 * @param head Next free position in the packet buffer. One TSPRT is written there for each piece of the sprite.
 * @param idx Portrait sprite to draw. An index past the end of the table draws nothing.
 * @param x Screen x of the sprite's origin. Each piece is drawn at its own offset from it.
 * @param y Screen y of the sprite's origin.
 * @param color Colour that multiplies the texture: red, green and blue in the low three bytes, where 0x80 leaves it unchanged.
 * @return Next free position in the packet buffer, after the packets written.
 */
void *DrawPortrait(void *ot, TSPRT *head, s32 idx, s32 x, s32 y, u32 color) {
    void *data = &g_portraitsTable;

    if (idx >= g_portraitsTable.spriteCount) {
        return head;
    }

    data += g_portraitsTable.offsets[idx];
    idx = ((Sprite *)data)->cellCount;
    data = ((Sprite *)data)->cells;
    while (idx > 0) {
        setSpriteCell(head, (SpriteCell *)data, x, y, color);
        addPrimFast(ot, head);
        head++;
        idx--;
        data = (SpriteCell *)data + 1;
    }
    return head;
}

/**
 * @brief Wrapper for DrawPortrait but always passes color 0x64808080.
 */
void *DrawPortraitNormalTint(void *ot, void *head, s32 idx, s32 x, s32 y) {
    return DrawPortrait(ot, head, idx, x, y, 0x64808080);
}

/**
 * @brief Draw sprite @p idx of a sprite table at a screen position.
 *
 * @param ot Ordering-table entry the packets are linked into.
 * @param head Next free position in the packet buffer. One TSPRT is written there for each piece of the sprite.
 * @param table Sprite table to draw from.
 * @param idx Sprite to draw. An index past the end of the table draws nothing.
 * @param x Screen x of the sprite's origin. Each piece is drawn at its own offset from it.
 * @param y Screen y of the sprite's origin.
 * @param color Colour that multiplies the texture: red, green and blue in the low three bytes, where 0x80 leaves it unchanged.
 * @return Next free position in the packet buffer, after the packets written.
 */
void *DrawSpriteFromTable(void *ot, TSPRT *head, SpriteTable *table, s32 idx, s32 x, s32 y, u32 color) {
    void *data = table;

    if (idx >= table->spriteCount) {
        return head;
    }

    data += table->offsets[idx];
    idx = ((Sprite *)data)->cellCount;
    data = ((Sprite *)data)->cells;
    while (idx > 0) {
        setSpriteCell(head, (SpriteCell *)data, x, y, color);
        addPrimFast(ot, head);
        head++;
        idx--;
        data = (SpriteCell *)data + 1;
    }
    return head;
}

/**
 * @brief Wrapper for DrawSpriteFromTable but always passes color 0x64808080.
 */
void *DrawSpriteFromTableNormalTint(void *ot, void *head, SpriteTable *table, s32 idx, s32 x, s32 y) {
    return DrawSpriteFromTable(ot, head, table, idx, x, y, 0x64808080);
}
