#ifndef EFFECT_EFFECT_025_MOTES_H
#define EFFECT_EFFECT_025_MOTES_H

#include "effect.h"

/**
 * @file effect_025_motes.h
 * @brief Cura: the rising, falling and arc mote scripts.
 */

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 */
s32 func_801A533C(EffectEntity *entity);

/** @brief Script dispatcher: the falling-mote script, drawn spun and tinted. */
s32 func_801A54DC(EffectEntity *entity);

/** @brief Script dispatcher: the rising-mote script, drawn tinted. */
s32 func_801A5C24(EffectEntity *entity);

#endif /* EFFECT_EFFECT_025_MOTES_H */
