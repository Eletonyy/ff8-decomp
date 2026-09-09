#ifndef EFFECT_EFFECT_036_H
#define EFFECT_EFFECT_036_H

#include "effect.h"
#include "effect/lib/particle.h"
#include "effect/lib/tint.h"

/**
 * @file
 * @brief Confuse: the overlay's own storage.
 */

/** @brief Cursor the record banks are carved off, moving up as they are taken. */
extern u8 *D_801E2300;

/**
 * @brief Value the script feeds to @c D_800F02C2, one entry per frame.
 *
 * Ends with @ref EFFECT_TABLE_END rather than carrying a length.
 */
extern s16 D_801A9DF8[];

/** @brief Sound cue the dispatcher fires on its first frame. */
extern u8 D_801A9E10[];

/** @brief Argument the frame-20 child is started with. */
extern EffectTintStep D_801A9E04[];

/** @brief Pose script the model-taking handler runs. */
extern EffectPoseTables D_801B2DE0;

/** @brief Task pools the script's children run in, and their storage. */
extern s32 D_801E2EB0;
extern s32 D_801E3020;
extern s32 D_801E30F0;
extern s32 D_801E2590;
extern s32 D_801E2DE0;
extern s32 D_801E2EC0;
extern s32 D_801E3030;
extern s32 D_801E25C0;
extern s32 D_801E2310;

/** @brief Cleared when the effect starts. */
extern s32 D_801E22F0;
extern s32 D_801E22F4;

/**
 * @name Overlay blob
 *
 * The data carried in the overlay image behind the code.
 * @{
 */

/** @brief Table of two offsets battle.bin resolves when the effect starts. */
extern u8 D_801B89C8;

/**
 * @brief TIM uploaded to VRAM when the effect starts.
 *
 * Once it is in VRAM the image memory is reused: the prim and frame banks the
 * renderer double-buffers through start at this address.
 */
extern u8 D_801B96D0;
/** @} */

/**
 * @name Prim buffers
 *
 * Two of each, so the frame being built and the frame being drawn never share.
 * @{
 */
extern void *D_801E2DC8;
extern void *D_801E2DCC;
extern void *D_801E2DD0;
extern void *D_801E2DD4;
/** @} */

/** @brief Cursor the effect's second prim buffer is written through. */
extern void *D_801E2DC4;

/**
 * @brief Run one frame of the effect: its opcode, then every task pool it owns.
 */
s32 func_801A9B64(EffectEntity *entity);

#endif /* EFFECT_EFFECT_036_H */
