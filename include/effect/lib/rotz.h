#ifndef EFFECT_LIB_ROTZ_H
#define EFFECT_LIB_ROTZ_H

#include "effect.h"

/**
 * @file rotz.h
 * @brief Rotation about the Z axis.
 */

/** @brief Turn @p m about Z by @p angle. */
void effectMatrixRotZ(MATRIX *m, s32 angle);

#endif /* EFFECT_LIB_ROTZ_H */
