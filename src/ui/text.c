#include "common.h"
#include "psxsdk/libgpu.h"
#include "ui/dialog.h"
#include "ui/text.h"
#include "numstr.h"
#include "ui/font.h"
#include "btl_anim.h"
#include "btl_anim_packet.h"
#include "ui/icon.h"
#include "menu_tint.h"

/** @brief Text starts this many pixels right of and below the window's text origin. */
#define DIALOG_TEXT_MARGIN 2

/** @brief Lines that hold choices are indented by this much, room for the cursor. */
#define DIALOG_CHOICE_INDENT 32

/** @brief Bit of a text glyph number that selects the font's second texture page. */
#define TEXT_GLYPH_PAGE2 0x400

/** @brief Glyph index within its texture page. */
#define TEXT_GLYPH_INDEX_MASK 0x3FF

/** @brief Draw-mode words for the font's first and second texture page. */
#define TEXT_TPAGE_PAGE1 0xE100041F
#define TEXT_TPAGE_PAGE2 0xE100041D

extern u32 g_textBlinkTint; // Same as g_dialogs.state.textBlinkTint
static TSPRT *drawTextIcon(P_TAG *ot, TSPRT *head, s32 idx, s32 x, s32 y);
static inline u32 addTextGlyph(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy);
static s32 getCharWidth(s32 idx);
static s32 measureText(u8 *s, s32 flag);


/**
 * @brief Draw one icon of @c g_iconTable as a run of sprites.
 *
 * Emits every cell of icon @p idx of @c g_iconTable as one
 * @ref TSPRT, tinted with @c state.textTint and linked into @p ot. Per cell:
 * the u/v/CLUT word is the cell's own plus the icons' CLUT; the texture page is
 * the icons' page with the cell's blend rate; the colour word gets the cell's
 * semi-transparency bit; width/height are copied and the cell's signed offsets
 * are added to (@p x, @p y).
 *
 * @note @c head is handed to @c p and taken back for the return, the way
 * @ref drawNextPageMarker threads its packet cursor; returning @c p directly
 * moves the cursor copy in the prologue. The @c (u8) narrowing of the
 * blend rate keeps @c _get_mode's mask, see @ref drawNextPageMarker.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param head First free packet.
 * @param idx Icon index into @c g_iconTable.
 * @param x Left edge of the glyph.
 * @param y Top edge of the glyph.
 * @return The first free packet after the ones written.
 */
static TSPRT *drawTextIcon(P_TAG *ot, TSPRT *head, s32 idx, s32 x, s32 y) {
    IconTable *table;
    IconCell *cell;
    TSPRT *p;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    u32 color;
    s32 n;

    p = head;
    table = &g_iconTable;
    cell = (IconCell *)table; /* seeded from its own copy, see drawNextPageMarker */
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);
    color = g_dialogs.state.textTint;

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & ICON_UVCLUT_MASK;
        val += getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
        setIconUVClut(p, val);

        val = (word >> ICON_ABR_SHIFT) & ICON_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y);

        val = word >> ICON_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
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
    head = p;
    return head;
}


/**
 * @brief Compute the bounding width of a multi-cell glyph.
 *
 * Walks glyph @p idx's cells and tracks the unsigned running maximum of each
 * cell's right edge (width + signed X offset) and bottom edge (height + signed
 * Y offset), returning the peak width (low byte).
 *
 * @note The peak height is computed but unused by the return value — the caller
 * presumably only needs the advance width. Purpose inferred from the
 * neighbouring glyph-metric routines.
 *
 * @param idx Icon index into @c g_iconTable.
 * @return Bounding width of the icon, masked to 8 bits.
 */
s32 getIconWidth(s32 idx) {
    IconCell *cell = (IconCell *)&g_iconTable;
    u32 word = g_iconTable.descriptors[idx];
    s32 cellCount = word >> 16;
    s32 maxWidth = 0;
    s32 maxHeight = 0;

    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);
    while (cellCount != 0) {
        s32 width, height;
        word = cell->metrics;
        width = word & 0xFF;
        width += (s8)(word >> 8);
        height = (word >> 16) & 0xFF;
        height += (s8)(word >> 24);
        if ((u32)maxWidth < (u32)width) {
            maxWidth = width;
        }
        if ((u32)maxHeight < (u32)height) {
            do { maxHeight = height; } while (0);
        }
        cellCount--;
        cell++;
    }
    return maxWidth & 0xFF;
}


/** @brief Extracts a 4-bit nibble from packed byte array D_800834D8.
 * Even indices return the low nibble; odd indices return the high nibble.
 * @note inline: drawDialogText has it expanded in place.
 * @param idx Nibble index.
 * @return The 4-bit value (0-15).
 */
inline s32 getNibbleValue(s32 idx) {
    u8 *base = D_800834D8;
    u32 val = base[idx >> 1];
    if (idx & 1) {
        val >>= 4;
    }
    return val & 0xF;
}


/**
 * @brief Get the width of character code @p idx from the glyph width table.
 *
 * Folds the character code @p idx into a glyph index depending on which of
 * three ranges it falls in (< 0x100, < 0x1C00, or higher — the last range is
 * offset and tagged with bit 0x400), then reads the packed nibble table
 * @c D_800834D8 exactly like @ref getNibbleValue: even indices take the low
 * nibble, odd indices the high nibble.
 *
 * @param idx Character code.
 * @return The 4-bit table value (0-15).
 */
static s32 getCharWidth(s32 idx) {
    u8 *base;
    u32 val;

    if (idx < 0x100) {
        idx -= 0x20;
    } else if (idx < 0x1C00) {
        idx -= 0x1820;
    } else {
        idx -= 0x1C20;
        idx |= 0x400;
    }

    base = D_800834D8;
    val = base[idx >> 1];
    if (idx & 1) {
        val >>= 4;
    }
    return val & 0xF;
}


/**
 * @brief Measure a battle-message string's packed {width, height}.
 *
 * Scans the string @p s, interpreting control codes (1/2/7 = line breaks,
 * 5 = special glyph resolved via @c func_8002C734 / @c getIconWidth,
 * 0x18-0x1F = multi-byte glyphs) and accumulating each line's pixel width from
 * the packed nibble-width table @ref D_800834D8. Tracks the widest line and the
 * line count; codes below 0x10 (other than 1/2/5/7) consume a following
 * argument byte. When @p flag is 0 the scan stops at the first line break.
 *
 * @param s Null-terminated message string.
 * @param flag If nonzero, measure every line; if zero, stop at the first break.
 * @return Packed dimensions: width in the low 16 bits, @c (lines*16 - 4) in the
 * high 16 bits.
 */
static s32 measureText(u8 *s, s32 flag) {
    s32 width = 0;
    s32 maxWidth = 0;
    s32 lines = 1;
    s32 maxLines = 1;
    s32 height;

    for (;;) {
        s32 c = *s++;
        if (c == 0) {
            if (maxWidth < width) {
                maxWidth = width;
            }
            if (maxLines < lines) {
                maxLines = lines;
            }
            height = maxLines * 16;
            break;
        }
        if ((u32) (c - 1) < 2 || c == 7) {
            /* control codes 1, 2, 7: line break */
            lines++;
            if (c == 1) {
                lines = 1;
            }
            if (c == 7) {
                lines = 1;
            }
            if (maxLines < lines) {
                maxLines = lines;
            }
            if (maxWidth < width) {
                maxWidth = width;
            }
            width = 0;
            if (flag == 0) {
                height = maxLines * 16;
                break;
            }
        } else if (c == 5) {
            c = *s++;
            width += getIconWidth(func_8002C734(c));
            width++;
        } else if (c < 0x10) {
            s++;
        } else if (c >= 0x18) {
            u8 packed;
            s32 nib;
            if (c >= 0x20) {
                c = c - 0x20;
            } else if (c < 0x1C) {
                c *= 0xE0;
                c += *s++;
                c -= 0x1520;
            } else {
                c *= 0xE0;
                c += *s++;
                c -= 0x18A0;
                c |= 0x400;
            }
            packed = D_800834D8[c >> 1];
            if (c & 1) {
                packed >>= 4;
            }
            nib = packed & 0xF;
            width += nib;
        }
    }

    return maxWidth | (((maxLines * 16) - 4) << 16);
}


/**
 * @brief Measure a message: the widest of its decoded lines and its height.
 *
 * Decodes the message one line at a time (nextMessageLine steps to the next,
 * NULL after the last) into a buffer taken from a $gp area set up on the
 * display-list head, and keeps the widest line. The height comes from
 * measuring the raw message as a whole.
 *
 * @param str Message in the game's text encoding.
 * @return Width in the low 16 bits, height in the high 16 bits (see
 * measureText).
 */
s32 measureMessage(u8 *str) {
    u8 *tempGp;
    u8 *savedGp;
    s32 head;
    u8 *buf;
    u8 *ret;
    s32 height;
    s32 maxWidth;
    s32 width;

    height = measureText(str, 1) >> 16;
    height <<= 16;
    head = getDisplayListHead();
    GP_SAVE_SET(tempGp, head);
    savedGp = tempGp;
    GP_ALLOC(buf, DIALOG_MSG_BUF_SIZE);
    maxWidth = 0;
    do {
        decodeMessage(str, buf, -1);
        width = measureText(buf, 0) & 0xFFFF;
        if (maxWidth < width) {
            maxWidth = width;
        }
        str = nextMessageLine(str);
    } while (str != NULL);
    GP_FREE(DIALOG_MSG_BUF_SIZE);
    GP_RESTORE_RET(savedGp, ret);
    return maxWidth | height;
}


/**
 * @brief Measure a text: the width of its widest line and its height.
 * @param str Text in the game's encoding.
 * @return Width in the low 16 bits, height in the high 16 bits (see measureText).
 */
s32 getTextSize(u8 *str) {
    return measureText(str, 1);
}


/**
 * @brief Same as getTextSize.
 * @param str Text in the game's encoding.
 * @return Width in the low 16 bits, height in the high 16 bits.
 */
s32 getTextSizeB(u8 *str) {
    return measureText(str, 1);
}


/**
 * @brief Get the width of a text's widest line.
 * @param str Text in the game's encoding.
 * @return Width in pixels.
 */
s32 getTextWidth(u8 *str) {
    return measureText(str, 1) & 0xFFFF;
}


/**
 * @brief Get the width of a text's first line.
 * @param str Text in the game's encoding.
 * @return Width in pixels.
 */
s32 getFirstLineWidth(u8 *str) {
    return measureText(str, 0) & 0xFFFF;
}


/**
 * @brief Set the menu brightness: store a grey in @c g_menuTint[MENU_TINT_NORMAL], the tint of
 * everything the menus draw, and refresh the blink copies.
 *
 * The raw value is kept in @c g_menuBrightness, where menus read it back to restore it.
 *
 * @param brightness Brightness, @ref BRIGHTNESS_NORMAL = normal; shifted down by 5,
 * its low 8 bits become r, g and b (0x80 draws graphics unmodulated).
 */
void setMenuBrightness(s32 brightness) {
    g_menuBrightness = brightness;
    {
        s32 val = (u32)brightness >> 5;
        brightness = val & 0xFF;
    }
    brightness |= (brightness << 16) | (brightness << 8);
    brightness |= SPRT_CODE;
    g_menuTint[MENU_TINT_NORMAL] = brightness;
    updateTextBlinkColors();
}


/**
 * @brief Fill one 12x12 text-font sprite in the menu colours and prepend it to the OT chain.
 *
 * Colours 0-7 use @c g_menuTint[MENU_TINT_NORMAL], 8-15 @c g_menuTint[MENU_TINT_BLINK];
 * the low 3 bits of the colour pick the CLUT row. Declared inline: drawDecodedText
 * has it expanded in place.
 *
 * @param head Current OT chain head (tag image of the previous packet).
 * @param p Sprite to fill.
 * @param glyph Text glyph number; @ref TEXT_GLYPH_PAGE2 selects the second texture page.
 * @param colour Text colour index (0-15).
 * @param xy Packed position, x in the low half and y in the high half.
 * @return The new chain head (@p p's tag image).
 */
inline u32 emitTextGlyph(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy) {
    u32 tag;
    u32 hi;
    u32 tpage;

    setlen(p, 5);
    addOtTagFast(p, head, tag);
    head = tag;
    hi = colour >> 3;
    colour &= 7;
    p->clut = (colour << 6) + getClut(TEXT_CLUT_X, TEXT_CLUT_Y);
    if (hi != 0) {
        colour = g_menuTint[MENU_TINT_BLINK];
    } else {
        colour = g_menuTint[MENU_TINT_NORMAL];
    }
    if (glyph & TEXT_GLYPH_PAGE2) {
        glyph &= TEXT_GLYPH_INDEX_MASK;
        tpage = TEXT_TPAGE_PAGE2;
    } else {
        tpage = TEXT_TPAGE_PAGE1;
    }
    setIconWH(p, (TEXT_GLYPH_SIZE << 16) | TEXT_GLYPH_SIZE);
    setIconRGBC(p, colour);
    *(u32 *)&p->x0 = xy;
    p->drawMode = tpage;
    *(u16 *)&p->u0 = (glyph % TEXT_GLYPHS_PER_ROW | (glyph / TEXT_GLYPHS_PER_ROW) << 8) * TEXT_GLYPH_SIZE;
    return head;
}


/**
 * @brief Draw a text string with the 12x12 font into the packets at @p p.
 *
 * Emits one sprite per glyph with emitTextGlyph, starting at (@p x, @p y).
 * Newlines return to @p x and move down a line; any other code below 0x19 ends
 * the string. Nothing is drawn when @p y is off screen, and the string stops at
 * the right screen edge. Unlike drawMessageText the text is already decoded.
 *
 * @param ot OT slot the sprites are linked into.
 * @param p First free packet.
 * @param x Left edge in pixels.
 * @param y Top edge in pixels.
 * @param str Decoded text, or NULL to draw nothing.
 * @param colour Text colour index (0-15).
 * @return The first packet after the ones drawn.
 */
TSPRT *drawDecodedText(P_TAG *ot, TSPRT *p, s32 x, s32 y, u8 *str, s32 colour) {
    u32 head;
    s32 startX;
    s32 c;

    if (str == NULL) {
        return p;
    }
    getAddrNewFast(ot, head);
    startX = x;
    if (y >= 0x101) { /* below the screen */
        return p;
    }
    if (y < -8) { /* above the screen */
        return p;
    }
    for (;;) {
        c = *str++;
        if (c == MSG_NEWLINE) {
            x = startX;
            y += DIALOG_LINE_HEIGHT;
            continue;
        }
        if (c < 0x19) {
            break;
        }
        if (x >= 0x181) { /* past the right screen edge */
            break;
        }
        if (c >= 0x20) { /* one-byte glyph */
            c -= 0x20;
        } else if (c < 0x1C) { /* two-byte glyph, first page */
            c *= 0xE0;
            c += *str++;
            c -= 0x1520;
        } else { /* two-byte glyph, second page */
            c *= 0xE0;
            c += *str++;
            c -= 0x18A0;
            c |= TEXT_GLYPH_PAGE2;
        }
        head = emitTextGlyph(head, p, c, colour, (y << 16) | (x & 0xFFFF));
        p++;
        x += getNibbleValue(c);
    }
    setAddrFast(ot, head);
    return p;
}


/**
 * @brief Fill one 12x12 text-font sprite and prepend it to the OT chain.
 *
 * The text renderers' counterpart of emitTextGlyph: colours 0-7 use @c state.textTint,
 * 8-15 (the blinking ones) @c g_textBlinkTint, and the low 3 bits of the colour
 * pick the CLUT row.
 *
 * @param head Current OT chain head (tag image of the previous packet).
 * @param p Sprite to fill.
 * @param glyph Text glyph number; @ref TEXT_GLYPH_PAGE2 selects the second texture page.
 * @param colour Text colour index (0-15).
 * @param xy Packed position, x in the low half and y in the high half.
 * @return The new chain head (@p p's tag image).
 */
static inline u32 addTextGlyph(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy) {
    u32 tag;
    u32 hi;
    u32 tpage;

    setlen(p, 5);
    addOtTagFast(p, head, tag);
    head = tag; /* taken here rather than at the return, as the original's copy order needs */
    hi = colour >> 3;
    colour &= 7;
    p->clut = (colour << 6) + getClut(TEXT_CLUT_X, TEXT_CLUT_Y);
    if (hi != 0) {
        colour = g_textBlinkTint;
    } else {
        colour = g_dialogs.state.textTint;
    }
    if (glyph & TEXT_GLYPH_PAGE2) {
        glyph &= TEXT_GLYPH_INDEX_MASK;
        tpage = TEXT_TPAGE_PAGE2;
    } else {
        tpage = TEXT_TPAGE_PAGE1;
    }
    setIconWH(p, (TEXT_GLYPH_SIZE << 16) | TEXT_GLYPH_SIZE);
    setIconRGBC(p, colour);
    *(u32 *)&p->x0 = xy;
    p->drawMode = tpage;
    *(u16 *)&p->u0 = (glyph % TEXT_GLYPHS_PER_ROW | (glyph / TEXT_GLYPHS_PER_ROW) << 8) * TEXT_GLYPH_SIZE;
    return head;
}


/**
 * @brief Draw a text string at (@p x, @p y) with the message-window font.
 *
 * Decodes @p str one line at a time into a scratch buffer and emits a sprite
 * per glyph, starting in white. While it runs, $gp points at the scratchpad
 * (0x1F800300), which holds the decode buffer. Newlines return to @p x and move
 * down a line; icons (code 0x05) are drawn with drawTextIcon, colour codes
 * (0x06) switch the text colour, and a page break or the end of the text stops.
 *
 * @param ot OT slot the text is linked into.
 * @param x Left edge in pixels.
 * @param y Top edge in pixels.
 * @param str Encoded text, or NULL to draw nothing.
 * @return The scratchpad pointer $gp held while the text was drawn.
 */
u8 *drawMessageText(P_TAG *ot, s32 x, s32 y, u8 *str) {
    u8 *buf;
    s32 startX;
    u8 *savedGp;
    u8 *tempGp;
    u8 *ret;
    TSPRT *p;
    u32 head;
    u32 colour;
    u8 *s;
    s32 c;

    startX = x;
    GP_SAVE_SCRATCH(tempGp);
    savedGp = tempGp;
    colour = DIALOG_COLOR_WHITE;
    GP_ALLOC(buf, DIALOG_MSG_BUF_SIZE);
    p = (TSPRT *)getDisplayListHead();
    getAddrNewFast(ot, head);

    /* str is tested here and at the top of the loop, as in drawDialogText. */
    if (str != NULL) {
        for (;;) {
            if (str == NULL) {
                goto end;
            }
            decodeMessage(str, buf, -1);
            g_messageColor = colour;
            str = nextMessageLine(str);

            s = buf;
            for (;;) {
                c = *s++;
                if (c >= 0x19) {
                    if (c >= 0x20) { /* one-byte glyph */
                        c -= 0x20;
                    } else if (c < 0x1C) { /* two-byte glyph, first page */
                        c *= 0xE0;
                        c += *s++;
                        c -= 0x1520;
                    } else { /* two-byte glyph, second page */
                        c *= 0xE0;
                        c += *s++;
                        c -= 0x18A0;
                        c |= TEXT_GLYPH_PAGE2;
                    }
                    head = addTextGlyph(head, p, c, colour, (y << 16) | (x & 0xFFFF));
                    p++;
                    x += getNibbleValue(c);
                    continue;
                }
                if (c == MSG_NEWLINE) {
                    x = startX;
                    y += DIALOG_LINE_HEIGHT;
                    break;
                }
                if (c == MSG_NEW_PAGE || c == MSG_NEW_PAGE_MARKED || c == MSG_END) {
                    goto end;
                }
                if (c >= 0x10) { /* codes 0x10-0x18 take no argument */
                    continue;
                }
                if (c == MSG_CMD_ICON) {
                    setAddrFast(ot, head);
                    c = *s++;
                    c = func_8002C734(c);
                    p = drawTextIcon(ot, p, c, x, y);
                    x += getIconWidth(c);
                    x++;
                    getAddrNewFast(ot, head);
                    continue;
                }
                if (c == MSG_CMD_COLOR) {
                    colour = *s++;
                    colour &= 0xF;
                    continue;
                }
                s++; /* skip the argument of any other command */
            }
        }
    }
end:
    setAddrFast(ot, head);
    storeGpuPacket((u32)p);
    GP_FREE(DIALOG_MSG_BUF_SIZE);
    GP_RESTORE_RET(savedGp, ret);
    return ret;
}


/**
 * @brief Draw the text of a message window.
 *
 * Decodes the message one line at a time into a scratch buffer taken from the
 * $gp area and emits a sprite per glyph, starting at the window's text origin
 * (@c textX, @c textY) scrolled up by @c scrollY. Lines scrolled out above
 * the window are skipped, and lines @c firstChoice to @c lastChoice (the choices) are
 * indented for the cursor. Lines before @c typingLine are drawn whole; the line
 * being typed shows its first @c typedChars bytes. Icons (code 0x05) are
 * drawn as multi-cell glyphs with drawTextIcon, colour codes (0x06) switch the
 * text colour, and a page break or the end of the message stops the text.
 *
 * @param ot OT slot the text is linked into.
 * @param entry Message window.
 */
void drawDialogText(P_TAG *ot, Dialog *entry) {
    u8 *buf;
    s32 first;
    s32 last;
    u8 *str;
    TSPRT *p;
    s32 line;
    u32 head;
    s32 y;
    s32 x;
    u32 colour;
    u8 *s;
    s32 c;

    GP_ALLOC(buf, DIALOG_MSG_BUF_SIZE);
    first = entry->firstChoice;
    str = entry->dataPtr;
    last = entry->lastChoice;
    p = (TSPRT *)getDisplayListHead();
    line = 0;
    getAddrNewFast(ot, head);
    /* The scroll offset is read signed (lh) here; updateDialog counts it unsigned. */
    y = entry->textY + (DIALOG_TEXT_MARGIN - (s16)entry->scrollY);
    x = entry->textX + DIALOG_TEXT_MARGIN;
    colour = entry->pageColor;
    if (line >= first && line <= last) {
        x = entry->textX + DIALOG_TEXT_MARGIN + DIALOG_CHOICE_INDENT;
    }
    g_messageColor = entry->pageColor;

    while (y < -DIALOG_LINE_HEIGHT) {
        if (str == NULL) {
            break;
        }
        str = nextMessageLine(str);
        y += DIALOG_LINE_HEIGHT;
        colour = g_messageColor & 0xF;
        line++;
    }

    /* str is tested both here and at the top of the loop, as in the original.
     * The loop is left through goto: with break, gcc rotates the test to the
     * bottom, which the original does not do (98.48%). */
    if (str != NULL) {
        for (;;) {
            if (str == NULL) {
                goto end;
            }
            /* c doubles as the length limit, as the original's register does. */
            c = -1;
            if (line >= entry->typingLine) {
                c = entry->typedChars;
            }
            decodeMessage(str, buf, c);
            g_messageColor = colour;
            str = nextMessageLine(str);
            line++;

            s = buf;
            for (;;) {
                c = *s++;
                if (c >= 0x19) {
                    if (c >= 0x20) { /* one-byte glyph */
                        c -= 0x20;
                    } else if (c < 0x1C) { /* two-byte glyph, first page */
                        /* The second byte is read before the multiply: that is
                         * what schedules its load ahead of it, as in the original. */
                        s32 lo = *s++;

                        c *= 0xE0;
                        c += lo;
                        c -= 0x1520;
                    } else { /* two-byte glyph, second page */
                        s32 lo = *s++;

                        c *= 0xE0;
                        c += lo;
                        c -= 0x18A0;
                        c |= TEXT_GLYPH_PAGE2;
                    }
                    head = addTextGlyph(head, p, c, colour, (y << 16) | (x & 0xFFFF));
                    p++;
                    x += getNibbleValue(c);
                    continue;
                }
                if (c == MSG_NEWLINE) {
                    x = entry->textX + DIALOG_TEXT_MARGIN;
                    if (line >= first && line <= last) {
                        x = entry->textX + DIALOG_TEXT_MARGIN + DIALOG_CHOICE_INDENT;
                    }
                    y += DIALOG_LINE_HEIGHT;
                    break;
                }
                if (c == MSG_NEW_PAGE || c == MSG_NEW_PAGE_MARKED || c == MSG_END) {
                    goto end;
                }
                if (c >= 0x10) { /* codes 0x10-0x18 take no argument */
                    continue;
                }
                if (c == MSG_CMD_ICON) {
                    setAddrFast(ot, head);
                    c = *s++;
                    c = func_8002C734(c);
                    p = drawTextIcon(ot, p, c, x, y);
                    x += getIconWidth(c);
                    x++;
                    getAddrNewFast(ot, head);
                    continue;
                }
                if (c == MSG_CMD_COLOR) {
                    colour = *s++;
                    colour &= 0xF;
                    continue;
                }
                s++; /* skip the argument of any other command */
            }
        }
    }
end:
    setAddrFast(ot, head);
    storeGpuPacket((u32)p);
    GP_FREE(DIALOG_MSG_BUF_SIZE);
}
