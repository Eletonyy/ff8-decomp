#ifndef EFFECT_LIB_RENDER_H
#define EFFECT_LIB_RENDER_H

#include "effect.h"

/**
 * @file render.h
 * @brief The effect renderer: pose a model and draw its meshes.
 */

/** @brief Per-joint world matrices, one per joint of the posed skeleton. */
extern MATRIX g_effectJointOffsets[];

/**
 * @brief Compose each joint's matrix through the pose and the per-joint offset.
 */
void effectRenderPoseJoints(EffectSkeletonRef *ref, EffectPose *pose);

/**
 * @brief Rebase a table of absolute pointers into offsets from its own head.
 */
void effectRebasePointers(s32 *table);

/** @brief Reset a render request to its default pose, white, and unit scale. */
void effectRenderReset(EffectRender *render, void *pose, EffectRenderPart *part,
                       void *texture, BattleEffectSlot *source, s32 frame);

/**
 * @brief Point a render request at a texture page, CLUT and source rectangle.
 */
void effectRenderSetTexture(EffectRender *render, SVECTOR *pos, s16 tx, s16 ty,
                            s16 clutX, s16 clutY, s16 w, s16 h);

/** @brief Pose the effect's model and link its skeletons into the battle OT. */
void effectRenderModel(EffectRender *render);

#endif /* EFFECT_LIB_RENDER_H */
