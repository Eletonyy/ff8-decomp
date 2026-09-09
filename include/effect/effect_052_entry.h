#ifndef EFFECT_EFFECT_052_ENTRY_H
#define EFFECT_EFFECT_052_ENTRY_H

#include "effect.h"

/**
 * @file effect_052_entry.h
 * @brief X-Potion: the overlay entry point.
 */

/**
 * @brief Start the effect's script and hand back its task pool.
 *
 * @param animSet Animation set the effect plays.
 * @return The pool the root entity lives in.
 */
void *func_801A0000(EffectAnimSet *animSet);

#endif /* EFFECT_EFFECT_052_ENTRY_H */
