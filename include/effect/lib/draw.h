#ifndef EFFECT_LIB_DRAW_H
#define EFFECT_LIB_DRAW_H

#include "effect/lib/particle.h"

/**
 * @file draw.h
 * @brief Run the steps of the pose list and draw the particles they leave.
 */

/**
 * @brief Step one particle of the pose list, or retire it once its phase is over.
 */
void effectParticleStep(EffectParticle *particle);

/** @brief One less than @p particle's count, as a signed step. */
s16 effectEmitterCountStep(EffectEmitter *emitter);

/** @brief Draw every particle on the list that has a model and is still posing. */
void effectDrawParticles(void);

#endif /* EFFECT_LIB_DRAW_H */
