/**
 * @file rotz.c
 * @brief Rotation about the Z axis.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/rotz.h"

/** @brief Turn @p m about Z by @p angle. */
void effectMatrixRotZ(MATRIX *m, s32 angle) {
    EffectRotScratch *rot = func_800B3698(sizeof(EffectRotScratch));

    rot->sin = rsin(angle);
    rot->cos = rsin((angle + 0x400) & 0xFFF);
    rot->m.m[0][0] = rot->cos;
    rot->m.m[0][1] = -rot->sin;
    rot->m.m[0][2] = 0;
    rot->m.m[1][0] = rot->sin;
    rot->m.m[1][1] = rot->cos;
    rot->m.m[1][2] = 0;
    rot->m.m[2][0] = 0;
    rot->m.m[2][1] = 0;
    rot->m.m[2][2] = ONE;
    gte_MulMatrix0(m, &rot->m, m);
    func_800B36B8(sizeof(EffectRotScratch));
}
