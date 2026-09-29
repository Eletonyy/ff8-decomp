#include "common.h"
#include "psxsdk/libgpu.h"
#include "ui/icon.h"

/* --- Private functions --- */

static void buildRgbGpuColor(s32 r, s32 g, s32 b);

/**
 * @brief Draw one glyph of the font table as a run of sprites.
 *
 * Emits every cell of glyph @p idx of the @c D_80052A68 font table as one
 * @c TSPRT, tinted with @p color and linked into @p ot; an @p idx past the
 * table's glyph count draws nothing. Per cell: the u/v/CLUT word is the cell's
 * own plus the font CLUT; the texture page is the font page with the cell's
 * blend rate; the colour word gets the cell's semi-transparency bit and is
 * forced to a SPRT code; width/height are copied and the cell's signed offsets
 * are added to (@p x, @p y).
 *
 * @note The emitter of drawTextIcon (dialog.c) with drawDialogMarker's colour
 * masking and the same @c (u8) narrowing of the blend rate. @c p is taken from
 * @p head only after the glyph-count check: set before it, the table address
 * gets a register of its own instead of the cell cursor's.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param head First free packet.
 * @param idx Glyph index into @c D_80052A68.
 * @param x Left edge of the glyph.
 * @param y Top edge of the glyph.
 * @param color Colour word the cells are tinted with.
 * @return The first free packet after the ones written.
 */
void *func_8002FF34(void *ot, void *head, s32 idx, s32 x, s32 y, s32 color) {
    GlyphTable *table;
    GlyphCell *cell;
    TSPRT *p;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    s32 n;

    table = &D_80052A68;
    cell = (GlyphCell *)table;
    if (idx >= table->glyphCount) {
        return head;
    }
    p = head;
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (GlyphCell *)((u8 *)cell + word);

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & GLYPH_UVCLUT_MASK;
        val += getClut(GLYPH_CLUT_X, GLYPH_CLUT_Y) << 16;
        setGlyphUVClut(p, val);

        val = (word >> GLYPH_ABR_SHIFT) & GLYPH_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, GLYPH_TPAGE_X, GLYPH_TPAGE_Y);

        val = word >> GLYPH_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
        val &= SPRT_RGB_MASK;
        val |= SPRT_CODE;
        setGlyphRGBC(p, val);

        word = cell->metrics;
        val = word & GLYPH_WH_MASK;
        setGlyphWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    return p;
}


/**
 * @brief Build a packed grayscale GPU color and store to g_gpuColor.
 * @param intensity Scalar intensity value (divided by 32, masked to 8 bits).
 */
void buildGrayscaleGpuColor(s32 intensity) {
    intensity /= 32;
    intensity &= 0xFF;
    g_gpuColor = intensity | (intensity << 8) | (intensity << 16) | SPRT_CODE;
}


/**
 * @brief Build a packed RGB GPU color and store to g_gpuColor.
 * @param r Red intensity (divided by 32, masked to 8 bits).
 * @param g Green intensity.
 * @param b Blue intensity.
 */
static void buildRgbGpuColor(s32 r, s32 g, s32 b) {
    r /= 32;
    g /= 32;
    b /= 32;
    r &= 0xFF;
    g &= 0xFF;
    b &= 0xFF;
    g_gpuColor = r | (g << 8) | (b << 16) | SPRT_CODE;
}


/**
 * @brief Draw one glyph of the font table as a run of sprites, with a CLUT offset.
 *
 * The emitter of drawTextIcon (dialog.c), tinted with @p color, with @p clut
 * added to every cell's CLUT: the menus pass CLUT ids such as @c (row << 6) + 2
 * to draw a glyph through another palette row. There is no glyph-count check.
 *
 * @note @p p is the packet cursor itself; copying it to a local first moves the
 * copy out of the prologue.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param p First free packet.
 * @param idx Glyph index into @c D_80052A68.
 * @param x Left edge of the glyph.
 * @param y Top edge of the glyph.
 * @param color Colour word the cells are tinted with.
 * @param clut CLUT id added to each cell's CLUT.
 * @return The first free packet after the ones written.
 */
void *func_800300F8(void *ot, TSPRT *p, s32 idx, s32 x, s32 y, s32 color, s32 clut) {
    GlyphTable *table;
    GlyphCell *cell;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    s32 n;

    table = &D_80052A68;
    cell = (GlyphCell *)table;
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (GlyphCell *)((u8 *)cell + word);

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & GLYPH_UVCLUT_MASK;
        val += getClut(GLYPH_CLUT_X, GLYPH_CLUT_Y) << 16;
        val += clut << 16;
        setGlyphUVClut(p, val);

        val = (word >> GLYPH_ABR_SHIFT) & GLYPH_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, GLYPH_TPAGE_X, GLYPH_TPAGE_Y);

        val = word >> GLYPH_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
        setGlyphRGBC(p, val);

        word = cell->metrics;
        val = word & GLYPH_WH_MASK;
        setGlyphWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    return p;
}


/** @brief Call buildGrayscaleGpuColor with the default parameter value 0x1000. */
void setDefaultGpuColor(void) { buildGrayscaleGpuColor(0x1000); }


/** @brief Empty stub -- no operation. */
void iconStub(void) {
}
