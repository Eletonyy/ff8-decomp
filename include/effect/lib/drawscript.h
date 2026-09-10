#ifndef EFFECT_LIB_DRAWSCRIPT_H
#define EFFECT_LIB_DRAWSCRIPT_H

#include "effect/lib/particle.h"

/**
 * @file drawscript.h
 * @brief Run the draw list a script carries.
 */

/** @brief Running totals the draw list adds into every frame. */
extern u16 g_effectParticleTotal;
extern u16 g_effectLiveTotal;

/**
 * @brief Point the draw list at this script's tables and aim it at its slot.
 */
void effectDrawScriptStart(EffectDrawScript *script);

/** @brief Run one frame of the draw list @p script carries. */
s32 effectDrawScriptRun(EffectDrawScript *script);

/**
 * @brief Set the draw list's span from three times the slot's height, clamped.
 */
void effectDrawListSpan(s16 slot);

#endif /* EFFECT_LIB_DRAWSCRIPT_H */
