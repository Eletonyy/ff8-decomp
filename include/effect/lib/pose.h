#ifndef EFFECT_LIB_POSE_H
#define EFFECT_LIB_POSE_H

#include "effect.h"

/**
 * @file pose.h
 * @brief Walk a pose script and hand each posed part to a callback.
 */

/** @brief A posed model: a header naming the part list behind it. */
typedef struct {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 partsOffset;
} EffectPoseModel;

/**
 * @brief One posed part, rebuilt from the pose script for every draw.
 *
 * @ref effectPoseWalk zeroes this, walks the script into it and hands it to the
 * per-part callback; the fields past @c colour are the animation cursor.
 */
typedef struct {
    /* 0x00 */ s16 index;    /**< The part's own slot in the pose script. */
    /* 0x02 */ s16 mesh;     /**< Which of the model's meshes it draws. */
    /* 0x04 */ u32 unk004;   /**< The part entry's header word; @c mesh is its low byte. */
    /* 0x08 */ s16 pos[3];
    /* 0x0E */ u8 pad00E[0x10 - 0xE];
    /* 0x10 */ s16 rot[3];
    /* 0x16 */ u8 pad016[0x18 - 0x16];
    /* 0x18 */ s16 scale[3];
    /* 0x1E */ u8 pad01E[0x20 - 0x1E];
    /* 0x20 */ CVECTOR colour;
    /* 0x24 */ s16 depth;
    /* 0x26 */ s16 weight;   /**< Blend between @c frameA and @c frameB. */
    /* 0x28 */ s16 frameA;
    /* 0x2A */ s16 frameB;
} EffectPartPose; /* 0x2C */

/** @brief What one pose walk draws: the matrix, its colours and its texture. */
typedef struct {
    /* 0x00 */ MATRIX *m;
    /* 0x04 */ u32 *colours;
    /* 0x08 */ void *unk08;
    /* 0x0C */ s32 unk00C;
    /* 0x10 */ s16 unk010;
    /* 0x12 */ u8 pad012[0x14 - 0x12];
} EffectDrawRequest; /* 0x14 */

/** @brief What @ref effectPoseWalk hands each part it has posed to. */
typedef void (*EffectPartFn)(s32 *pose, EffectPartPose *part,
                             EffectDrawRequest *req);

/**
 * @brief Clear @p size bytes of pose state at @p out and seed it from @p model.
 */
void effectPoseInit(EffectPoseModel *model, s32 *out, s32 size);

/** @brief Walk one pose script and hand every part it poses to @p fn. */
s32 effectPoseWalk(s32 *pose, EffectPartFn fn, EffectDrawRequest *req);

#endif /* EFFECT_LIB_POSE_H */
