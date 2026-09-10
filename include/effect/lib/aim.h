#ifndef EFFECT_LIB_AIM_H
#define EFFECT_LIB_AIM_H

#include "effect.h"

/**
 * @file aim.h
 * @brief Matrices that aim along a direction.
 */

/**
 * @brief Build a matrix in @p m that aims along @p dir with @p up as the roll.
 */
void effectMatrixAim(MATRIX *m, SVECTOR *dir, SVECTOR *up);

/** @brief Aim @p m along the vector from @p from to @p to, Y up. */
void effectMatrixLookAt(MATRIX *m, SVECTOR *from, SVECTOR *to);

#endif /* EFFECT_LIB_AIM_H */
