#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "battle.h"
#include "input/button_remap.h"
#include "dialog.h"
#include "btl_entity.h"
#include "btl_anim.h"
#include "btl_display.h"
#include "drawbar.h"
#include "ui/icon.h"
#include "numstr.h"
#include "game.h"
#include "menu_tint.h"

/** @brief The code byte of a colour word: the primitive code and its option bits. */
#define COLOUR_WORD_CODE 0xFF000000

/** @brief A 15-bit texture page is 64 pixels wide and 256 lines tall. */
#define TPAGE_WIDTH 64
#define TPAGE_HEIGHT 256

extern BattleDisplayEntity g_battleEntities[];
extern s32 D_800834CC;
extern u8 g_digitBaseCode;
extern DisplayListBuf *D_800834C0;
extern u16 D_80052974[];

static void *func_8002BC6C(u32 *ot, s32 idx, void *head);
static u8 *func_8002BE48(u32 *ot, u8 *head);
static inline SPRT *drawSprtIcon(void *ot, s32 clut, SPRT *p, s32 idx, s32 x, s32 y, u32 color);


/**
 * @brief Draw a block of the active draw area again at @p rect.
 *
 * The sprites' texture is VRAM itself: 15-bit texture pages starting at the
 * active draw environment's clip origin, so @p rect shows the block of that
 * size from the draw area's top-left corner. @p rect is covered in strips one
 * texture page wide, each a ModeSprt whose draw mode selects the next page.
 *
 * @param ot OT entry to link into.
 * @param p Packet cursor.
 * @param rect Screen rect to cover.
 * @param color Colour word of the sprites; its semi-transparency bit is cleared.
 * @return The packet cursor after the sprites.
 */
ModeSprt *func_8002BAA0(u32 *ot, ModeSprt *p, RECT *rect, u32 color) {
    RECT clip;
    u32 mode;
    u32 texWindow;
    u32 uv;
    u32 link;
    s32 ty;
    s32 x;
    s32 y;
    s32 n;
    s32 w;
    s32 h;

    copyDisplayRect(&clip);
    color &= ~(SPRT_CODE_ABE << SPRT_CODE_SHIFT);
    /* x and y hold the clip origin, then the rect's: one pair keeps the original's registers */
    y = clip.y;
    x = clip.x;
    ty = y / TPAGE_HEIGHT * TPAGE_HEIGHT;
    mode = _get_mode(1, 0, getTPage(2, 0, x / TPAGE_WIDTH * TPAGE_WIDTH, ty));
    texWindow = TEXWINDOW_OFF; /* a variable: the original loads it before the OT read */
    uv = (x % TPAGE_WIDTH) | ((y - ty) << 8);
    getAddrNewFast(ot, link);
    x = rect->x;
    y = rect->y;
    n = rect->w;
    h = rect->h;
    for (; n > 0; n -= TPAGE_WIDTH) {
        w = n;
        if (w > TPAGE_WIDTH) {
            w = TPAGE_WIDTH;
        }
        setlen(p, 7);
        p->drawMode = mode;
        p->texWindow[0] = texWindow;
        p->texWindow[1] = 0;
        p->code = SPRT_CODE >> SPRT_CODE_SHIFT;
        *(u32 *)&p->r0 = color; /* r, g, b and code in one store, over the code just set */
        setXY0(p, x, y);
        setWH(p, w, h);
        *(u32 *)&p->u0 = uv; /* u, v and CLUT in one store */
        link = linkPacket(link, p);
        p++;
        mode++;
        x += TPAGE_WIDTH;
    }
    setAddrFast(ot, link);
    return p;
}


/**
 * @brief Draw a window box: its whole frame (func_8002B3A0 with both sides),
 * then its background (drawWindowBackground).
 * @param ot Ordering table.
 * @param prim Primitive buffer cursor.
 * @param rect The window rect.
 * @param color Colour word.
 * @return The primitive cursor after the packets.
 */
DR_AREA *func_8002BC10(P_TAG *ot, DR_AREA *prim, RECT *rect, s32 color)
{
    DR_AREA *p;

    p = func_8002B3A0(ot, prim, rect, color, WINDOW_FRAME_LEFT | WINDOW_FRAME_RIGHT);

    return drawWindowBackground(ot, p, rect, color);
}


/**
 * @brief Draw battle entity @p idx into the OT entry of its anim speed.
 *
 * Nothing is drawn while its bound rect is empty. Otherwise its render hook
 * runs, then a draw area and offset for its clamped rect (@c clipClamp) are
 * linked. An entity with a box that is not a dialog (whose hook draws its own)
 * also gets the box's frame and background, grey at its @c brightness with the
 * code byte of its @c drawMode, and a draw area and offset for its bound rect
 * (@c clipBound).
 *
 * @param ot Ordering table.
 * @param idx Entity index.
 * @param head Display-list write head.
 * @return The display-list head after the entity's packets.
 */
static void *func_8002BC6C(u32 *ot, s32 idx, void *head) {
    BattleDisplayEntity *e;
    EntityRenderCallback render;
    DR_AREA *p;
    RECT r;
    RECT *rect;
    u32 link;
    u32 size;
    u32 w;
    u32 h;
    u32 colour;
    u32 grey;
    s32 speed;

    speed = getBattleEntityAnimSpeed(idx);
    e = &g_battleEntities[idx];
    p = head;
    size = *(u32 *)&e->boundRect.w; /* w and h in one load, as the original reads them */
    render = e->render;
    ot = &ot[speed];
    h = size >> 16;
    w = size & 0xFFFF;
    if (w == 0 || h == 0) {
        return p;
    }
    if (render != NULL) {
        p = render(ot, e, p);
    }
    getAddrNewFast(ot, link);
    SetDrawArea(p, &e->clipClamp.rect);
    link = linkPacket(link, p);
    p++;
    SetDrawOffset((DR_OFFSET *)p, (u16 *)&e->clipClamp.savedPos); /* savedPos packs x and y */
    link = linkPacket(link, p);
    p++;
    if ((e->entityType & BATTLE_ENTITY_BOX) && !(e->entityType & BATTLE_ENTITY_DIALOG)) {
        colour = e->brightness;
        grey = colour >> 5;
        colour = grey | ((grey << 16) | (grey << 8));
        setAddrFast(ot, link);
        r.x = 0;
        r.y = 0;
        r.w = e->boundRect.w;
        r.h = e->boundRect.h;
        rect = &r; /* the original keeps &r in a register across both calls */
        colour |= e->drawMode & COLOUR_WORD_CODE;
        p = func_8002B3A0(ot, p, rect, colour, WINDOW_FRAME_LEFT | WINDOW_FRAME_RIGHT);
        p = drawWindowBackground(ot, p, rect, colour);
        getAddrNewFast(ot, link);
        SetDrawArea(p, &e->clipBound.rect);
        link = linkPacket(link, p);
        p++;
        SetDrawOffset((DR_OFFSET *)p, (u16 *)&e->clipBound.savedPos);
        link = linkPacket(link, p);
        p++;
    }
    setAddrFast(ot, link);
    return p;
}


/**
 * @brief Reset the draw area and draw offset to the active draw environment's.
 *
 * Emits a DR_AREA for the environment's clip rect and a DR_OFFSET for its draw
 * offset, and links both into @p ot.
 *
 * @param ot OT entry to link into.
 * @param head Display-list write head.
 * @return The display-list head after the two packets.
 */
static u8 *func_8002BE48(u32 *ot, u8 *head) {
    RECT clip;
    u16 ofs[2];
    u32 link;
    u32 tag;

    getAddrFast(ot, link);
    copyDisplayRect(&clip);
    copyDisplayCoords(ofs);
    SetDrawArea((DR_AREA *)head, &clip);
    link = linkPacket(link, head);
    head += sizeof(DR_AREA);
    SetDrawOffset((DR_OFFSET *)head, ofs);
    tag = linkPacket(link, head);
    head += sizeof(DR_OFFSET);
    setAddrFast(ot, tag);
    return head;
}


/**
 * @brief Get the OT entry for a battle entity based on its anim speed.
 * @param idx Entity index.
 * @return Pointer to the OT entry at active display list's ot[animSpeed].
 */
s32* getEntityTablePtr(s32 idx) {
    u32 *base = D_800834C0->ot;
    s32 speed = getBattleEntityAnimSpeed(idx);
    return &base[speed];
}


/**
 * @brief Build the per-frame battle entity display list.
 *
 * For each of the 8 entity slots, runs the visibility test (activeFlag set,
 * field36 clear, field35 set) and clips the entity rect via @c clipBlitRects.
 * Each entity that passes and has a non-empty clip result sets its bit in
 * @p mask. Then @c func_8002BE48 sets up the draw area at @c ot[15], and for
 * every set bit @c func_8002BC6C is called to render that entity into the
 * display list at @p head.
 *
 * @param ot Ordering table.
 * @param head Display-list write head; advanced through nested calls.
 * @return The display-list head after all entity primitives are emitted.
 */
u8 *func_8002BF24(u32 *ot, u8 *head) {
    s32 mask = 0;
    s32 hit;
    s32 i;
    s32 bit;
    s32 slotBit;

    i = 0;
    bit = 1;
    for (; i < 8; i++) {
        BattleDisplayEntity *e = &g_battleEntities[i];
        hit = 0;
        if (e->activeFlag != 0 && e->unk36 == 0 && e->unk35 != 0) {
            hit = (clipBlitRects((BlitParams *)e) != 0);
        }
        if (hit) {
            mask |= bit << i;
        }
    }

    head = func_8002BE48(&ot[15], head);
    if (mask == 0) {
        return head;
    }

    for (i = 0; i < 8; i++) {
        slotBit = 1;
        if (mask & (slotBit << i)) {
            head = func_8002BC6C(ot, i, head);
        }
    }
    return head;
}


/**
 * @brief Dispatch a battle entity's update callback.
 *
 * If the entity has a callback set, invokes it with the entity pointer.
 *
 * @param idx Entity index.
 */
void dispatchBattleEntity(s32 idx) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    if (entity->callback) {
        entity->callback(entity);
    }
}


/** @brief Finds the first available battle entity slot (0-7) and marks it as active.
 *  @return Slot index (0-7), or -1 if none available.
 */
s32 allocBattleEntitySlot(void) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (GetActiveFlag(i) == 0) {
            setBattleEntityActive(i, 1);
            return i;
        }
    }
    return -1;
}


/**
 * @brief Initialize all 8 battle entity slots by calling initBattleEntity on each.
 *
 * Iterates slots 0 through 7, resetting each entity's display rects, flags,
 * position fields, and brightness to default values.
 */
void initAllBattleEntities(void) {
    s32 i;
    for (i = 0; i < 8; i++) {
        initBattleEntity(i);
    }
}


/**
 * @brief Store a base address for battle entity data.
 * @param val Value to store (typically a memory address).
 */
void setBattleEntityBase(s32 val) {
    D_800834CC = val;
}


/** @brief Return the maximum number of battle entity slots (always 8). */
s32 getMaxBattleEntities(void) {
    return 8;
}


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


/**
 * @brief Set a dialog's text origin.
 * @param idx Dialog index.
 * @param x Text origin x in pixels (@c textX).
 * @param y Text origin y in pixels (@c textY).
 */
void setDialogTextOrigin(s32 idx, s32 x, s32 y) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->textX = x;
    entry->textY = y;
}


/**
 * @brief Set a dialog's choice lines.
 * @param idx Dialog index.
 * @param first First choice line (@c firstChoice).
 * @param last Last choice line (@c lastChoice).
 * @param cancel Line Triangle moves the cursor to, negative for none (@c cancelChoice).
 */
void setDialogChoices(s32 idx, s32 first, s32 last, s32 cancel) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->firstChoice = first;
    entry->lastChoice = last;
    entry->ctrl.bits.cancelChoice = cancel;
}


/**
 * @brief Put a dialog's choice cursor on a line (@c choiceCursor).
 * @param idx Dialog index.
 * @param val Choice line.
 */
void setDialogChoiceCursor(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->choiceCursor = val;
}


/**
 * @brief Set a dialog's draw hook.
 * @param idx Dialog index.
 * @param val Hook to run, or NULL for none.
 */
void setDialogDrawCallback(s32 idx, DialogDrawCallback val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->drawCallback = val;
}


/**
 * @brief Set a dialog's per-frame update hook.
 * @param idx Dialog index.
 * @param val Hook to run, or NULL for none.
 */
void setDialogUpdateCallback(s32 idx, DialogCallback val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->updateCallback = val;
}


/**
 * @brief Set a message window's brightness and copy it to the window's display entity.
 * @param idx Dialog index.
 * @param val Brightness (0x1000 = full).
 */
void setDialogBrightness(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->brightness = val;
    setBattleEntityBrightness(entry->entityIdx, val);
}


