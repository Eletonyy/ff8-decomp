#ifndef EFFECT_LIB_STEP_H
#define EFFECT_LIB_STEP_H

#include "effect.h"

/**
 * @file step.h
 * @brief Script counter stepping.
 */

/** @brief Advance the script's counter, clamping at its limit. */
s32 effectStepCounter(EffectEntity *entity);

#endif /* EFFECT_LIB_STEP_H */
