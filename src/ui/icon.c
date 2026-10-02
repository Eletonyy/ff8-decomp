#include "common.h"
#include "psxsdk/libgpu.h"
#include "battle_anim.h"
#include "ui/icon.h"

/* --- Private functions --- */

static void setNextPageMarkerColor(s32 r, s32 g, s32 b);

/**
 * @brief Draw one icon of @c g_iconTable as a run of sprites.
 *
 * Emits every cell of icon @p idx of @c g_iconTable as one
 * @c TSPRT, tinted with @p color and linked into @p ot; an @p idx past the
 * table's icon count draws nothing. Per cell: the u/v/CLUT word is the cell's
 * own plus the icons' CLUT; the texture page is the icons' page with the cell's
 * blend rate; the colour word gets the cell's semi-transparency bit and is
 * forced to a SPRT code; width/height are copied and the cell's signed offsets
 * are added to (@p x, @p y).
 *
 * @note The emitter of drawTextIcon (text.c) with drawNextPageMarker's colour
 * masking and the same @c (u8) narrowing of the blend rate. @c p is taken from
 * @p head only after the icon-count check: set before it, the table address
 * gets a register of its own instead of the cell cursor's.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param head First free packet.
 * @param idx Icon index into @c g_iconTable.
 * @param x Left edge of the icon.
 * @param y Top edge of the icon.
 * @param color Colour word the cells are tinted with.
 * @return The first free packet after the ones written.
 */
void *drawIcon(void *ot, void *head, s32 idx, s32 x, s32 y, s32 color) {
    IconTable *table;
    IconCell *cell;
    TSPRT *p;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    s32 n;

    table = &g_iconTable;
    cell = (IconCell *)table;
    if (idx >= table->iconCount) {
        return head;
    }
    p = head;
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);

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
        val &= SPRT_RGB_MASK;
        val |= SPRT_CODE;
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
 * @brief Build a packed grayscale GPU color and store to g_engine.nextPageMarkerColor.
 * @param intensity Scalar intensity value (divided by 32, masked to 8 bits).
 */
void setNextPageMarkerBrightness(s32 intensity) {
    intensity /= 32;
    intensity &= 0xFF;
    g_engine.nextPageMarkerColor = intensity | (intensity << 8) | (intensity << 16) | SPRT_CODE;
}


/**
 * @brief Build a packed RGB GPU color and store to g_engine.nextPageMarkerColor.
 * @param r Red intensity (divided by 32, masked to 8 bits).
 * @param g Green intensity.
 * @param b Blue intensity.
 */
static void setNextPageMarkerColor(s32 r, s32 g, s32 b) {
    r /= 32;
    g /= 32;
    b /= 32;
    r &= 0xFF;
    g &= 0xFF;
    b &= 0xFF;
    g_engine.nextPageMarkerColor = r | (g << 8) | (b << 16) | SPRT_CODE;
}


/**
 * @brief Draw one icon of @c g_iconTable as a run of sprites, with a CLUT offset.
 *
 * The emitter of drawTextIcon (text.c), tinted with @p color, with @p clut
 * added to every cell's CLUT: the menus pass CLUT ids such as @c (row << 6) + 2
 * to draw an icon through another palette row. There is no icon-count check.
 *
 * @note @p p is the packet cursor itself; copying it to a local first moves the
 * copy out of the prologue.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param p First free packet.
 * @param idx Icon index into @c g_iconTable.
 * @param x Left edge of the icon.
 * @param y Top edge of the icon.
 * @param color Colour word the cells are tinted with.
 * @param clut CLUT id added to each cell's CLUT.
 * @return The first free packet after the ones written.
 */
void *drawIconClut(void *ot, TSPRT *p, s32 idx, s32 x, s32 y, s32 color, s32 clut) {
    IconTable *table;
    IconCell *cell;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    s32 n;

    table = &g_iconTable;
    cell = (IconCell *)table;
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
    return p;
}


/** @brief Call setNextPageMarkerBrightness with the default parameter value 0x1000. */
void resetNextPageMarkerBrightness(void) { setNextPageMarkerBrightness(0x1000); }


/** @brief Empty stub -- no operation. */
void iconStub(void) {
}
