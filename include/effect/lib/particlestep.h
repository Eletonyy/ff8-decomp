#ifndef EFFECT_LIB_PARTICLESTEP_H
#define EFFECT_LIB_PARTICLESTEP_H

#include "effect/lib/particle.h"

/**
 * @file particlestep.h
 * @brief Advance, age and pose the copies of a particle.
 */

/** @brief Run the late step on every particle whose source is in the right mode. */
void effectParticleLateStep(void);

/**
 * @brief Advance every copy of @p particle: spin it, drift it and turn its pose.
 */
void effectParticleAdvance(EffectParticle *particle, EffectPoseStep *step);

/**
 * @brief Age @p particle's hold on its current step and advance when it expires.
 */
void effectParticleAgeHold(EffectParticle *particle, EffectPoseStep *step);

/** @brief Build @p particle's pose: orient it, colour it, scale it and place it. */
void effectParticlePose(EffectParticle *particle, EffectPoseStep *step);

#endif /* EFFECT_LIB_PARTICLESTEP_H */
