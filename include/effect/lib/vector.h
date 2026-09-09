#ifndef EFFECT_LIB_VECTOR_H
#define EFFECT_LIB_VECTOR_H

#include "effect/lib/particle.h"

/**
 * @file vector.h
 * @brief Random draws, angle jitter and vector helpers for the particle engine.
 */

/** @brief A random value between @p lo and @p hi. */
s32 effectRandBetween(s32 lo, s32 hi);

/**
 * @brief A random value between @p lo and @p hi, from a product of two draws.
 */
s32 effectRandProduct(s32 lo, s32 hi);

/**
 * @brief Combine @p a and @p b component-wise through @ref effectRandProduct.
 */
void effectRandVector(VECTOR *out, VECTOR *a, VECTOR *b);

/**
 * @brief Build @p m from @p angles, applying only the turns that are non-zero.
 */
void effectMatrixFromAngles(VECTOR *angles, MATRIX *m);

/**
 * @brief Combine @p a and @p b component-wise through @ref effectJitterAngle.
 */
void effectJitterVector(VECTOR *out, VECTOR *a, VECTOR *b);

/**
 * @brief Combine @p a and @p b component-wise through @ref effectJitterAngleS.
 */
void effectJitterSVector(SVECTOR *out, SVECTOR *a, SVECTOR *b);

/** @brief Copy the three words at @p src to @p dst. */
void effectCopyVector(s32 *src, s32 *dst);

/** @brief Fade @p value towards zero by @p t, a 16.16 fraction of it. */
void effectFadeValue(s32 t, s32 *value);

/** @brief Fade all three components of @p v towards zero by @p t. */
void effectFadeVector(s32 t, VECTOR *v);

/**
 * @brief Blend this frame's two source point sets into the particle's target set.
 */
void effectBlendPointSets(EffectParticle *particle, EffectPoseStep *step);

#endif /* EFFECT_LIB_VECTOR_H */
