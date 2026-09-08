#ifndef EFFECT_EFFECT_025_ANIM_H
#define EFFECT_EFFECT_025_ANIM_H

#include "effect.h"

/**
 * @file effect_025_anim.h
 * @brief Cura: draw an animation frame at the entity, upright or rolled.
 */

/**
 * @brief Draw @p anim upright at the entity's position, tinted with @p colour.
 */
void func_801A4FE0(EffectEntity *entity, BattleSpriteAnim *anim,
                   CVECTOR *colour);

/** @brief As @ref func_801A4FE0, rolled by @p angle and scaled by @p scale. */
void func_801A50D4(EffectEntity *entity, BattleSpriteAnim *anim,
                   CVECTOR *colour, s16 angle, s16 scale);

#endif /* EFFECT_EFFECT_025_ANIM_H */
