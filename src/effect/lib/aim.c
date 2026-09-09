/**
 * @file aim.c
 * @brief Matrices that aim along a direction.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/aim.h"

/** @brief Build a matrix in @p m that aims along @p dir with @p up as the roll. */
void effectMatrixAim(MATRIX *m, SVECTOR *dir, SVECTOR *up) {
    MATRIX *basis = func_800B3698(sizeof(MATRIX));
    SVECTOR *row1 = (SVECTOR *)basis->m[1];
    SVECTOR *row2 = (SVECTOR *)basis->m[2];

    /* A matrix row is 6 bytes, so the whole-vector store spills one halfword
       into row 2 -- harmless, row 2 is written next. The width is load-bearing:
       it is the unaligned 8-byte copy the target does here, where the
       element-wise stores below are three halfwords. */
    *row1 = *up;
    row2->vx = dir->vx;
    row2->vy = dir->vy;
    row2->vz = dir->vz;
    gte_ldopv1SV(row1);
    gte_ldopv2SV(row2);
    gte_op12();
    gte_stsv(basis->m[0]);
    MatrixNormal_2(basis, basis);
    TransposeMatrix(basis, m);
    func_800B36B8(sizeof(MATRIX));
}

/** @brief Aim @p m along the vector from @p from to @p to, Y up. */
void effectMatrixLookAt(MATRIX *m, SVECTOR *from, SVECTOR *to) {
    SVECTOR up;
    SVECTOR delta;

    up.vx = 0;
    up.vy = ONE;
    up.vz = 0;
    delta.vx = to->vx - from->vx;
    delta.vy = to->vy - from->vy;
    delta.vz = to->vz - from->vz;
    effectMatrixAim(m, &delta, &up);
}
