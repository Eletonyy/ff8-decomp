#ifndef UI_ICON_H
#define UI_ICON_H

#include "common.h"
#include "battle.h"

/* --- Glyph font table --- */

/**
 * @brief One 8-byte sprite cell of a glyph in the @c D_80052A68 font table.
 *
 * A glyph is drawn from one or more of these cells, and both words are baked in
 * the layout the GPU packet wants so the emitters only mask and add.
 *
 * @c texInfo: bits 0-15 are u and v; bits 16-19 and 22-26 are the two pieces of
 * a CLUT offset that is added to the font CLUT (@ref GLYPH_UVCLUT_MASK keeps
 * exactly these and u/v); bit 27 is the semi-transparency flag and bits 30-31
 * the blend rate. Bit 21 is set in every cell of the shipped table, but no
 * emitter reads it.
 *
 * @c metrics packs four bytes: the sprite width, a signed X offset, the sprite
 * height and a signed Y offset, so width + X offset is the cell's right edge
 * and height + Y offset its bottom edge.
 */
typedef struct {
    /* 0x00 */ u32 texInfo; /**< u | v<<8 | CLUT offset bits | abe<<27 | abr<<30. */
    /* 0x04 */ u32 metrics; /**< w | (s8)xOffset<<8 | h<<16 | (s8)yOffset<<24. */
} GlyphCell;

/**
 * @brief Header view of the @c D_80052A68 font table (baked into executable data).
 *
 * A glyph count followed by one descriptor per glyph. Each descriptor packs the
 * glyph's cell @c count (high 16 bits) and the byte offset from the table base
 * to that glyph's @ref GlyphCell list (low 16 bits). The cell lists themselves
 * live in the trailing area the descriptors point at.
 */
typedef struct {
    /* 0x00 */ s32 glyphCount;
    /* 0x04 */ u32 descriptors[1]; /**< cellCount<<16 | byteOffsetToCells. */
} GlyphTable;

/** @brief Font glyph table (glyph count + per-glyph descriptors + cell lists). */
extern GlyphTable D_80052A68;

/** @brief u, v and CLUT-offset bits of @c GlyphCell.texInfo. */
#define GLYPH_UVCLUT_MASK 0x07CFFFFF

/** @brief Width and height bytes of @c GlyphCell.metrics. */
#define GLYPH_WH_MASK 0x00FF00FF

/** @brief Shift that brings the blend rate of @c GlyphCell.texInfo (bits 30-31)
 * down to bit 0. */
#define GLYPH_ABR_SHIFT 30

/** @brief Width of the blend rate once shifted down. getTPage masks again, but
 * dropping this one costs the match. */
#define GLYPH_ABR_MASK 3

/** @brief Shift that lands the semi-transparency flag of @c GlyphCell.texInfo
 * (bit 27) on @ref SPRT_CODE_ABE. */
#define GLYPH_ABE_SHIFT 26

/** @brief Semi-transparency option bit of a primitive's code byte. */
#define SPRT_CODE_ABE 0x02

/** @brief Position of the code byte inside the colour word. */
#define SPRT_CODE_SHIFT 24

/** @brief r, g, b and the two option bits of the code byte in the colour word. */
#define SPRT_RGB_MASK 0x03FFFFFF

/** @brief Primitive code 0x64 (SPRT) in the colour word. */
#define SPRT_CODE 0x64000000

/** @brief VRAM position of the font's CLUT row; a cell's CLUT offset is added to it. */
#define GLYPH_CLUT_X 256
#define GLYPH_CLUT_Y 224

/** @brief VRAM position of the font's texture page. */
#define GLYPH_TPAGE_X 896
#define GLYPH_TPAGE_Y 256

/* Whole-word setters for a TSPRT's r0/g0/b0/code, u0/v0/clut and w/h groups: the
 * glyph cells hold those groups ready-made, so they are stored in one piece.
 * The do/while(0) of setGlyphUVClut is load-bearing: the scheduler moves nothing
 * across it, which keeps the u/v/CLUT store ahead of the texture-page code.
 * Wrapping the colour setter the same way breaks drawDialogMarker's match. */
#define setGlyphRGBC(p, word) (*(u32 *)&(p)->r0 = (word))
#define setGlyphUVClut(p, word) do { *(u32 *)&(p)->u0 = (word); } while (0)
#define setGlyphWH(p, word) (*(u32 *)&(p)->w = (word))

/* --- Data externs (sorted by address) --- */

extern s32              g_gpuColor;           /* 0x800834C8 */

extern void buildGrayscaleGpuColor(s32 intensity);
extern void setDefaultGpuColor(void);
extern void iconStub(void);

void *func_8002FF34(void *ot, void *head, s32 idx, s32 x, s32 y, s32 color);
void *func_800300F8(void *ot, TSPRT *p, s32 idx, s32 x, s32 y, s32 color, s32 clut);


#endif
