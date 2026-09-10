#ifndef EFFECT_LIB_ENTITY_H
#define EFFECT_LIB_ENTITY_H

#include "effect.h"

/**
 * @file entity.h
 * @brief Effect entities: task spawning, battle-slot anchors and model bounds.
 */

/** @brief Opcode handler: release one frame of the linked script's wait. */
void effectReleaseWait(EffectEntity *entity);

/** @brief Allocate an effect entity from @p pool and hang it off @p owner. */
void *effectSpawnTask(void *pool, void *task, s32 stride, EffectEntity *owner);

/** @brief Cache the battle slot's two anchor points and their midpoint. */
void effectCacheSlotAnchors(EffectEntity *entity);

/** @brief Copy @c unk030 of the linked model out to @p out. */
void effectSlotAnchor0(EffectEntity *entity, SVECTOR *out);

/** @brief Copy @c unk038 of the linked model out to @p out. */
void effectSlotAnchor1(EffectEntity *entity, SVECTOR *out);

/** @brief Copy @c unk040 of the linked model out to @p out. */
void effectSlotAnchorMid(EffectEntity *entity, SVECTOR *out);

/**
 * @brief As @ref effectCacheSlotAnchors, but for the slot the animation set
 *        names.
 */
void effectCacheAnimSlotAnchors(EffectEntity *entity);

/** @brief Copy @c unk030 of the linked model out to @p out. */
void effectAnimSlotAnchor0(EffectEntity *entity, SVECTOR *out);

/** @brief Copy @c unk038 of the linked model out to @p out. */
void effectAnimSlotAnchor1(EffectEntity *entity, SVECTOR *out);

/** @brief Copy @c unk040 of the linked model out to @p out. */
void effectAnimSlotAnchorMid(EffectEntity *entity, SVECTOR *out);

/** @brief Recompute the model's bounding box from its posed skeleton. */
void effectUpdateModelBounds(EffectEntity *entity);

/** @brief Copy the linked model's bounding box out to @p min and @p max. */
void effectGetModelBounds(EffectEntity *entity, SVECTOR *min, SVECTOR *max);

/** @brief Half the longest side of the model's bounding box. */
s32 effectModelRadius(EffectEntity *entity);

/** @brief Half the height of the linked model's bounding box. */
s16 effectModelHalfHeight(EffectEntity *entity);

/** @brief Half the linked model's larger horizontal extent. */
s16 effectModelHalfWidth(EffectEntity *entity);

/** @brief Height of the linked model's bounding box. */
s16 effectModelHeight(EffectEntity *entity);

/** @brief Y of the top of the linked model, in battle-entity space. */
s16 effectModelTop(EffectEntity *entity);

/** @brief Y of the bottom of the linked model, in battle-entity space. */
s16 effectModelBottom(EffectEntity *entity);

/** @brief A random point up the linked model, in battle-entity space. */
s16 effectModelRandomY(EffectEntity *entity);

/** @brief Y of the centre of the linked model, in battle-entity space. */
s16 effectModelCentreY(EffectEntity *entity);

/** @brief Centre of the linked model's bounding box. */
void effectModelCentre(EffectEntity *entity, SVECTOR *out);

/** @brief Zero @p size bytes' worth of words starting at @p dst. */
void effectZeroWords(void *dst, s32 size);

#endif /* EFFECT_LIB_ENTITY_H */
