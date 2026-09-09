#ifndef EFFECT_EFFECT_039_H
#define EFFECT_EFFECT_039_H

#include "effect.h"
#include "effect/lib/particle.h"
#include "effect/lib/tint.h"

/**
 * @file
 * @brief Drain: the overlay's own storage.
 */

extern s32 D_801DA474;

/** @brief Pose script the model-taking handler runs. */
extern EffectPoseTables D_801AC3C8;

/** @brief Pool the trailing sparks are allocated from. */
extern s32 D_801D99E4;

/** @brief Cursor the record banks are carved off, moving up as they are taken. */
extern u8 *D_801D9754;

/**
 * @name Overlay blob
 *
 * The data carried in the overlay image behind the code.
 * @{
 */

/** @brief Table of two offsets battle.bin resolves when the effect starts. */
extern u8 D_801B01EC;

/**
 * @brief TIM uploaded to VRAM when the effect starts.
 *
 * Once it is in VRAM the image memory is reused: the three prim and frame
 * banks the renderer double-buffers through start at this address.
 */
extern u8 D_801B0924;
/** @} */

/** @brief Task pools the script's children run in. */
extern s32 D_801DA304;
extern s32 D_801DA234;
extern s32 D_801DA314;
extern s32 D_801DA484;
extern s32 D_801D9A14;
extern s32 D_801D9764;

/** @brief Cleared when the effect starts. */
extern s32 D_801D9744;
extern s32 D_801D9748;

/**
 * @name Prim buffers
 *
 * Two of each, so the frame being built and the frame being drawn never share.
 * @{
 */
extern void *D_801DA21C;
extern void *D_801DA220;
extern void *D_801DA224;
extern void *D_801DA228;
/** @} */

/** @brief Cursor the effect's second prim buffer is written through. */
extern void *D_801DA218;

/** @brief Sound cue the take-model script fires on its first frame. */
extern u8 D_801AA010[];

/** @brief Argument the frame-20 child is started with. */
extern EffectTintStep D_801A9FE4[];
extern EffectTintStep D_801A9FF0[];

/** @brief Pool the frame-20 child is allocated from. */
extern s32 D_801DA544;

/**
 * @brief Run one frame of the effect: its opcode, then every task pool it owns.
 */
s32 func_801A9D2C(EffectEntity *entity);

#endif /* EFFECT_EFFECT_039_H */
