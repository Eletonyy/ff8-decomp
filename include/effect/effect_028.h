#ifndef EFFECT_EFFECT_028_H
#define EFFECT_EFFECT_028_H

#include "effect.h"
#include "effect/lib/particle.h"
#include "effect/lib/tint.h"

/**
 * @file
 * @brief Curaga: the overlay's own storage.
 */

extern s32 D_801D49FC;

/** @brief Pose script the model-taking handler runs. */
extern EffectPoseTables D_801D4AFC;

/** @brief Pool the trailing sparks are allocated from. */
extern s32 D_801D3F6C;

/** @brief Cursor the record banks are carved off, moving up as they are taken. */
extern u8 *D_801D3CDC;

/**
 * @name Overlay blob
 *
 * The data carried in the overlay image behind the code.
 * @{
 */

/** @brief Table of two offsets battle.bin resolves when the effect starts. */
extern u8 D_801AA10C;

/**
 * @brief TIM uploaded to VRAM when the effect starts.
 *
 * Once it is in VRAM the image memory is reused: the three prim and frame
 * banks the renderer double-buffers through start at this address.
 */
extern u8 D_801AACAC;
/** @} */

/** @brief Task pools the script's children run in. */
extern s32 D_801D488C;
extern s32 D_801D47BC;
extern s32 D_801D489C;
extern s32 D_801D4A0C;
extern s32 D_801D3F9C;
extern s32 D_801D3CEC;

/** @brief Cleared when the effect starts. */
extern s32 D_801D3CCC;
extern s32 D_801D3CD0;

/**
 * @name Prim buffers
 *
 * Two of each, so the frame being built and the frame being drawn never share.
 * @{
 */
extern void *D_801D47A4;
extern void *D_801D47A8;
extern void *D_801D47AC;
extern void *D_801D47B0;
/** @} */

/** @brief Cursor the effect's second prim buffer is written through. */
extern void *D_801D47A0;

/** @brief Sound cue the take-model script fires on its first frame. */
extern u8 D_801A9FAC[];

/** @brief Argument the frame-20 child is started with. */
extern EffectTintStep D_801A9FA0[];

/** @brief Pool the frame-20 child is allocated from. */
extern s32 D_801D4ACC;

/**
 * @brief Run one frame of the effect: its opcode, then every task pool it owns.
 */
s32 func_801A9CF4(EffectEntity *entity);

#endif /* EFFECT_EFFECT_028_H */
