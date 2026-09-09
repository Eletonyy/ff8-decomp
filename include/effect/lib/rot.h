#ifndef EFFECT_LIB_ROT_H
#define EFFECT_LIB_ROT_H

#include "effect.h"

/**
 * @file rot.h
 * @brief Rotation matrices built from an angle.
 */

/** @brief Build the Z, X then Y rotation of @p angles in @p out. */
void effectMatrixSetRotZXY(SVECTOR *angles, MATRIX *out);

#endif /* EFFECT_LIB_ROT_H */
