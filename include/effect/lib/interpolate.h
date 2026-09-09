#ifndef EFFECT_LIB_INTERPOLATE_H
#define EFFECT_LIB_INTERPOLATE_H

#include "effect.h"

/**
 * @file interpolate.h
 * @brief Interpolate a vertex animation between two of its frames.
 */

/** @brief A vertex animation: a header, then @c count posed points per frame. */
typedef struct {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 count;
    /* 0x08 */ s32 unk008;
    /* 0x0C */ SVECTOR frames[1];
} EffectVertexAnim;

/** @brief Interpolate every point of @p anim between two of its frames. */
void effectInterpolateFrames(EffectVertexAnim *anim, s32 frameA, s32 frameB,
                             s32 t, SVECTOR *out);

#endif /* EFFECT_LIB_INTERPOLATE_H */
