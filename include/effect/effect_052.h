#ifndef EFFECT_EFFECT_052_H
#define EFFECT_EFFECT_052_H

#include "effect.h"
#include "effect/lib/particle.h"
#include "effect/lib/tint.h"

/**
 * @file effect_052.h
 * @brief X-Potion: the overlay's own storage.
 */

/** @brief Argument the frame-20 child is started with. */
extern EffectTintStep D_801A9C68[];

/** @brief Sound cue the take-model script fires on its first frame. */
extern u8 D_801A9C74[];

/** @brief Pool the trailing sparks are allocated from. */
extern s32 D_801D4424;

/** @brief Pool the frame-20 child is allocated from. */
extern s32 D_801D44F4;

/** @brief Pose script the model-taking handler runs. */
extern EffectPoseTables D_801D4524;

/** @brief Cleared when the effect starts. */
extern s32 D_801D3994;
extern s32 D_801D3998;

/** @brief Task pools the script's children run in. */
extern s32 D_801D42B4;
extern s32 D_801D41E4;
extern s32 D_801D42C4;
extern s32 D_801D4434;
extern s32 D_801D39C4;

/** @brief Table of two offsets battle.bin resolves when the effect starts. */
extern u8 D_801A9DD4;

/** @brief TIM uploaded to VRAM when the effect starts; also the first bank. */
extern u8 D_801AA974;

/** @brief Cursor the record banks are carved off, moving up as they are taken. */
extern u8 *D_801D39A4;

/** @brief Cursor the effect's second prim buffer is written through. */
extern void *D_801D41C8;

/** @brief The four bank pointers the emitters and the renderer read. */
extern void *D_801D41CC;
extern void *D_801D41D0;
extern void *D_801D41D4;
extern void *D_801D41D8;

/** @brief Run one frame of the effect: its opcode, then every task pool it owns. */
s32 func_801A99EC(EffectEntity *entity);

#endif /* EFFECT_EFFECT_052_H */
