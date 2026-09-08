#ifndef EFFECT_LIB_SPAWN_H
#define EFFECT_LIB_SPAWN_H

#include "effect/lib/particle.h"

/**
 * @file spawn.h
 * @brief Spawn particles off emitters and place emitters on the points their sources name.
 */

/** @brief Step one emitter: set it up once, run its kind, then age it. */
void effectEmitterStep(EffectEmitter *emitter);

/**
 * @brief Put @p emitter on the strand point its source names, plus that source's
 *        offset.
 */
void effectEmitterOnStrand(EffectEmitter *emitter);

/**
 * @brief Put @p emitter on the trail point its source names, plus that source's
 *        offset.
 */
void effectEmitterOnTrail(EffectEmitter *emitter);

/**
 * @brief Put @p emitter on the node that owns it, plus that source's turned
 *        offset.
 */
void effectEmitterOnOwner(EffectEmitter *emitter);

/** @brief Slide @p emitter between the two points its source names. */
void effectEmitterSlide(EffectEmitter *emitter);

#endif /* EFFECT_LIB_SPAWN_H */
