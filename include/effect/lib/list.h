#ifndef EFFECT_LIB_LIST_H
#define EFFECT_LIB_LIST_H

#include "effect/lib/particle.h"

/**
 * @file list.h
 * @brief The draw list itself, and the per-frame setup of the emitters on it.
 */

/**
 * @brief Give @p particle the count @p value and put it on the tail of the draw
 *        list.
 */
void effectListAppend(EffectParticle *particle, s16 value);

/** @brief Take @p particle off the draw list, mending both links. */
void effectListRemove(EffectParticle *particle);

/** @brief Empty the draw list. */
void effectListClear(void);

/** @brief Run every record on the draw list through the step its kind wants. */
void effectListRun(void);

/**
 * @brief Set @p emitter up for this frame: turn it, place it, and reset its life.
 */
void effectEmitterSetup(EffectEmitter *emitter);

/** @brief Fire each of @p step's four slots that is armed. */
void effectStepFireSlots(EffectParticle *particle, EffectPoseStep *step);

/** @brief Allocate an emitter for every source whose cue falls on this frame. */
void effectEmittersOnCue(void);

/** @brief Push the script's current position onto the head of the trail. */
void effectTrailPush(EffectDrawScript *script);

#endif /* EFFECT_LIB_LIST_H */
