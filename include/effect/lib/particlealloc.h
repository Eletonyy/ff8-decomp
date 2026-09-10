#ifndef EFFECT_LIB_PARTICLEALLOC_H
#define EFFECT_LIB_PARTICLEALLOC_H

#include "effect/lib/particle.h"

/**
 * @file particlealloc.h
 * @brief Allocate a particle from its fixed pool of slots.
 */

/** @brief Allocate a particle from its pool and put it on the draw list. */
EffectParticle *effectParticleAlloc(EffectEmitter *owner, u8 source);

#endif /* EFFECT_LIB_PARTICLEALLOC_H */
