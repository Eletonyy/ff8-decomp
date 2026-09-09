#ifndef EFFECT_EFFECT_027_H
#define EFFECT_EFFECT_027_H

#include "effect.h"
#include "effect/lib/particle.h"

/**
 * @file
 * @brief Full-Life: the overlay's own storage.
 */

/** @brief Cursor the record banks are carved off, moving up as they are taken. */
extern u8 *D_801D35C4;

/** @brief Sound cue the script fires on its first frame. */
extern u8 D_801A9C5C[];

/** @brief Pose script the model-taking handler runs. */
extern EffectPoseTables D_801D4314;

/** @brief Task pools the script's children run in, and their storage. */
extern s32 D_801D4174;
extern s32 D_801D42E4;
extern s32 D_801D3854;
extern s32 D_801D40A4;
extern s32 D_801D4184;
extern s32 D_801D3884;
extern s32 D_801D35D4;

/** @brief Cleared when the effect starts. */
extern s32 D_801D35B4;
extern s32 D_801D35B8;

/**
 * @name Prim buffers
 *
 * Two of each, so the frame being built and the frame being drawn never share.
 * @{
 */
extern void *D_801D408C;
extern void *D_801D4090;
extern void *D_801D4094;
extern void *D_801D4098;
/** @} */

/** @brief Cursor the effect's second prim buffer is written through. */
extern void *D_801D4088;

/**
 * @name Overlay blob
 *
 * The data carried in the overlay image behind the code.
 * @{
 */

/** @brief Table of two offsets battle.bin resolves when the effect starts. */
extern u8 D_801A9D3C;

/** @brief TIM uploaded to VRAM when the effect starts. */
extern u8 D_801AA794;
/** @} */

/**
 * @brief Run one frame of the effect: its opcode, then every task pool it owns.
 */
s32 func_801A99C8(EffectEntity *entity);

#endif /* EFFECT_EFFECT_027_H */
