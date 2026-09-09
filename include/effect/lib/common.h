#ifndef EFFECT_LIB_COMMON_H
#define EFFECT_LIB_COMMON_H

#include "effect.h"

/**
 * @file common.h
 * @brief Debug text, screen quads and the basic matrix builders.
 */

/** @brief Cursor this frame's prims are written through. */
extern void *g_effectPrimCursor;
/** @brief The sixteen hex digit glyphs, cached from the main string table. */
extern u8 g_effectHexGlyphs[];
/** @brief Debug text cursor: X, Y, and the colour the glyphs are drawn in. */
extern s32 g_effectTextX;
extern s32 g_effectTextY;
extern s32 g_effectTextColour;

/**
 * @brief Cache the sixteen hex digit glyphs and reset the debug text cursor.
 */
void effectTextInit(void);

/**
 * @brief Draw @p value as eight hex glyphs at the current debug text cursor.
 */
void effectTextHex(u32 value);

/** @brief Move the debug text cursor to the start of the next line. */
void effectTextNewline(void);

/** @brief Opcode handler: no-op. */
void effectNop(EffectEntity *entity);

/** @brief Emit a 128x128 textured quad at the head of the prim buffer. */
void effectDrawQuad128(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                       s32 abr);

/** @brief Emit a 256x256 textured quad at the head of the prim buffer. */
void effectDrawQuad256(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                       s32 abr);

/** @brief Load the identity matrix. */
void effectMatrixIdentity(MATRIX *m);

/** @brief Post-multiply @p m by a rotation of @p angle about X. */
void effectMatrixRotX(MATRIX *m, s32 angle);

/** @brief Compose a rotation about the Y axis into @p m. */
void effectMatrixRotY(MATRIX *m, s32 angle);

#endif /* EFFECT_LIB_COMMON_H */
