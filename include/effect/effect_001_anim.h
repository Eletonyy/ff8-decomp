#ifndef EFFECT_EFFECT_001_ANIM_H
#define EFFECT_EFFECT_001_ANIM_H

#include "effect.h"

/**
 * @file effect_001_anim.h
 * @brief Cure: draw an animation frame at the entity.
 */

/**
 * @brief Draw @p anim at the entity's position in @p colour and link it into
 *        the OT.
 */
void func_801A172C(EffectEntity *entity, BattleSpriteAnim *anim,
                   CVECTOR *colour);

#endif /* EFFECT_EFFECT_001_ANIM_H */
