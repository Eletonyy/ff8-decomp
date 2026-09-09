#ifndef EFFECT_LIB_EMIT_MODE_H
#define EFFECT_LIB_EMIT_MODE_H

#include "effect.h"

/**
 * @file emit_mode.h
 * @brief The mesh prim emitters whose prims carry a draw-mode word.
 */

/** @brief Draw-mode command word every emitted prim carries. */
extern u32 g_effectDrawMode;

/**
 * @name Prim emitter flags -- @ref EffectPrimBuild::flags.
 * @{
 */
#define EFFECT_EMIT_SEMITRANS 0x1
#define EFFECT_EMIT_OPAQUE 0x4
#define EFFECT_EMIT_TWO_SIDED 0x10
#define EFFECT_EMIT_DEPTH_CUE 0x40

/* The gouraud emitters read their own copy of each of the four bits above. */
#define EFFECT_EMIT_G_SEMITRANS 0x2
#define EFFECT_EMIT_G_OPAQUE 0x8
#define EFFECT_EMIT_G_TWO_SIDED 0x20
#define EFFECT_EMIT_G_DEPTH_CUE 0x80
#define EFFECT_EMIT_TPAGE_SET 0x100
#define EFFECT_EMIT_CLUT_SET 0x200
#define EFFECT_EMIT_TPAGE_ADD 0x400
#define EFFECT_EMIT_CLUT_ADD 0x800
#define EFFECT_EMIT_UNK1000 0x1000
#define EFFECT_EMIT_UNK2000 0x2000
/** @} */

/** @brief What the prim emitters walk: the mesh stream and the GTE results. */
typedef struct {
    /* 0x00 */ s32 *stream;
    /* 0x04 */ u32 *verts;
    /* 0x08 */ u8 r;              /**< Far colour the depth cue fades toward. */
    /* 0x09 */ u8 g;
    /* 0x0A */ u8 b;
    /* 0x0B */ u8 pad00B[0xC - 0xB];
    /* 0x0C */ s32 depth;         /**< Depth-cue weight handed to DPCS. */
    /* 0x10 */ u16 tpage;        /**< Texture page the textured prims take. */
    /* 0x12 */ u8 pad012[0x14 - 0x12];
    /* 0x14 */ u16 clut;         /**< CLUT id the textured prims take. */
    /* 0x16 */ u8 pad016[0x18 - 0x16];
    /* 0x18 */ s32 unk018;
    /* 0x1C */ u32 flags;
    /* 0x20 */ s32 *cursor;       /**< Walks the stream, one entry per prim kind. */
    /* 0x24 */ s32 nclip;
    /* 0x28 */ u8 pad028[0x2C - 0x28];
    /* 0x2C */ s32 otz;
    /* 0x30 */ u32 gteFlag;
} EffectPrimBuild; /* 0x34 */

/** @brief The flat triangle an emitter writes; six words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ u16 x1;
    /* 0x0E */ u16 y1;
    /* 0x10 */ u16 x2;
    /* 0x12 */ u16 y2;
    /* 0x14 */ u32 unk014;
} EffectTri; /* 0x18 */

/**
 * @brief One gouraud textured quad in the mesh stream.
 *
 * The last two UV pairs share a word, so the build's offset is doubled into
 * both halves and the sum split again on the way out.
 */
typedef struct {
    /* 0x00 */ u32 colour0;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u16 idx3;
    /* 0x0C */ u32 uv0;
    /* 0x10 */ u32 uv1;
    /* 0x14 */ u32 uv23;
    /* 0x18 */ u32 colour1;
    /* 0x1C */ u32 colour2;
    /* 0x20 */ u32 colour3;
} EffectEmitGouraudTexQuad; /* 0x24 */

/** @brief Run every prim kind of one mesh stream into the ordering table. */
EffectTri *effectEmitMesh(EffectPrimBuild *s, u32 *ot, s32 otShift,
                          EffectTri *prim);

#endif /* EFFECT_LIB_EMIT_MODE_H */
