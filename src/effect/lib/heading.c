/**
 * @file heading.c
 * @brief The heading a matrix faces.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/heading.h"

/** @brief The heading @p m faces, a quarter turn ahead of its Z axis. */
s32 effectMatrixHeading(MATRIX *m) {
    return (ratan2(m->m[2][2], m->m[0][2]) + 0x400) & 0xFFF;
}
