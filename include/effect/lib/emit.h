#ifndef EFFECT_LIB_EMIT_H
#define EFFECT_LIB_EMIT_H

#include "effect/lib/particle.h"

/**
 * @file emit.h
 * @brief The mesh prim emitters that write four-word prims.
 */

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
/** @brief Set, the vertex buffer the stream names is already resolved. */
#define EFFECT_EMIT_VERTS_SET 0x2000
/** @brief Set, the emitters keep the depth the packet came in with. */
#define EFFECT_EMIT_KEEP_DEPTH 0x1000
/** @} */

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
} EffectTri; /* 0x14 */

/**
 * @brief What @ref effectEmitStream walks: the mesh stream and how to shade it.
 *
 * Built on the scratchpad stack @ref g_effectStackTop walks. The overlay reserves
 * battle's 0x58 bytes for it even though it fills far less.
 */
typedef struct {
    /* 0x00 */ s32 *stream;           /**< The mesh the packet draws. */
    /* 0x04 */ u32 *verts;            /**< Where the projected points are kept. */
    /* 0x08 */ EffectPrimColor color; /**< Far colour the depth cue fades toward. */
    /* 0x0C */ s32 depth;             /**< Depth-cue weight handed to DPCS. */
    /* 0x10 */ u16 tpage;             /**< Texture page the textured prims take. */
    /* 0x12 */ u8 pad012[0x14 - 0x12];
    /* 0x14 */ u16 clut;              /**< CLUT id the textured prims take. */
    /* 0x16 */ u8 pad016[0x18 - 0x16];
    /* 0x18 */ s32 unk018;
    /* 0x1C */ u32 flags;
    /* 0x20 */ s32 *cursor;           /**< Walks the stream, one entry per prim kind. */
    /* 0x24 */ s32 nclip;
    /* 0x28 */ u8 pad028[0x2C - 0x28];
    /* 0x2C */ s32 otz;
    /* 0x30 */ u32 gteFlag;
    /* 0x34 */ u8 pad034[0x58 - 0x34];
} EffectPrimBuild; /* 0x58 */

/**
 * @brief Walk @p prim's mesh stream, letting each emitter draw its own prims.
 */
EffectTri *effectEmitStream(EffectPrimBuild *prim, u32 *ot, s32 otShift,
                            EffectTri *head);

#endif /* EFFECT_LIB_EMIT_H */
