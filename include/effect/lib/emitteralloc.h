#ifndef EFFECT_LIB_EMITTERALLOC_H
#define EFFECT_LIB_EMITTERALLOC_H

#include "effect/lib/particle.h"

/**
 * @file emitteralloc.h
 * @brief Allocate an emitter from its fixed pool of slots.
 */

/** @brief Allocate an emitter from its pool and put it on the draw list. */
EffectEmitter *effectEmitterAlloc(void *owner, s16 source, s16 index);

#endif /* EFFECT_LIB_EMITTERALLOC_H */
