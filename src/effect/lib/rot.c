/**
 * @file rot.c
 * @brief Rotation matrices built from an angle.
 *
 * The three builders clear the matrix through a countdown whose index is never
 * read, which makes it look like a loop that should count up. It is not: the
 * ascending form generates different code, so the countdown is load-bearing.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/rot.h"

static void effectMatrixSetRotX(s16 angle, MATRIX *m);
static void effectMatrixSetRotY(s16 angle, MATRIX *m);
static void effectMatrixSetRotZ(s16 angle, MATRIX *m);

/** @brief Build the rotation of @p angle about X in @p m. */
static void effectMatrixSetRotX(s16 angle, MATRIX *m) {
    s32 *p = (s32 *)m;
    s32 i;
    s16 sin;
    s16 cos;

    for (i = 5; i != 0; i--) {
        *p++ = 0;
    }
    sin = rsin(angle);
    cos = rcos(angle);
    m->m[0][0] = ONE;
    m->m[1][1] = cos;
    m->m[2][1] = -sin;
    m->m[1][2] = sin;
    m->m[2][2] = cos;
}

/** @brief Build the rotation of @p angle about Y in @p m. */
static void effectMatrixSetRotY(s16 angle, MATRIX *m) {
    s32 *p = (s32 *)m;
    s32 i;
    s16 sin;
    s16 cos;

    for (i = 5; i != 0; i--) {
        *p++ = 0;
    }
    sin = rsin(angle);
    cos = rcos(angle);
    m->m[2][0] = sin;
    m->m[0][0] = cos;
    m->m[1][1] = ONE;
    m->m[0][2] = -sin;
    m->m[2][2] = cos;
}

/** @brief Build the rotation of @p angle about Z in @p m. */
static void effectMatrixSetRotZ(s16 angle, MATRIX *m) {
    s32 *p = (s32 *)m;
    s32 i;
    s16 sin;
    s16 cos;

    for (i = 5; i != 0; i--) {
        *p++ = 0;
    }
    sin = rsin(angle);
    cos = rcos(angle);
    m->m[1][0] = -sin;
    m->m[0][0] = cos;
    m->m[1][1] = cos;
    m->m[0][1] = sin;
    m->m[2][2] = ONE;
}

/** @brief Build the Z, X then Y rotation of @p angles in @p out. */
void effectMatrixSetRotZXY(SVECTOR *angles, MATRIX *out) {
    MATRIX rz;
    MATRIX rx;
    MATRIX ry;
    MATRIX zx;

    effectMatrixSetRotZ(angles->vz, &rz);
    effectMatrixSetRotX(angles->vx, &rx);
    MulMatrix0(&rz, &rx, &zx);
    effectMatrixSetRotY(angles->vy, &ry);
    MulMatrix0(&zx, &ry, out);
}
