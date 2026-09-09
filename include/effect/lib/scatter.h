#ifndef EFFECT_LIB_SCATTER_H
#define EFFECT_LIB_SCATTER_H

#include "effect.h"

/**
 * @file scatter.h
 * @brief Push an entity to a random point.
 */

/** @brief Push @p entity to a random point within @p radius of where it is. */
void effectScatter(EffectEntity *entity, s16 radius);

#endif /* EFFECT_LIB_SCATTER_H */
