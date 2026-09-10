#ifndef EFFECT_LIB_STRANDS_H
#define EFFECT_LIB_STRANDS_H

#include "effect/lib/particle.h"

/**
 * @file strands.h
 * @brief Seed, push and aim the five strands.
 */

/** @brief Seed every point of all five strands from the list's seed point. */
void effectStrandsSeed(void);

/** @brief Push @p node's position onto the head of every strand. */
void effectStrandsPush(EffectDrawScript *script);

/**
 * @brief Aim every strand at its own model part, then seed them from the
 *        middle.
 */
void effectStrandsAim(EffectDrawScript *script);

#endif /* EFFECT_LIB_STRANDS_H */
