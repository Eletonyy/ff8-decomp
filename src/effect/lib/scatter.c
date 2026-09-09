/**
 * @file scatter.c
 * @brief Push an entity to a random point.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/libc.h"
#include "effect/lib/common.h"
#include "effect/lib/scatter.h"

/** @brief Push @p entity to a random point within @p radius of where it is. */
void effectScatter(EffectEntity *entity, s16 radius) {
    MATRIX m;
    SVECTOR angles;
    SVECTOR in;
    SVECTOR out;

    if (radius == 0) {
        radius = 1;
    }
    angles.vy = rand() & 0xFFF;
    angles.vx = rand() & 0xFFF;
    effectMatrixIdentity(&m);
    effectMatrixRotY(&m, angles.vy);
    effectMatrixRotX(&m, angles.vx);
    in.vx = 0;
    in.vy = 0;
    in.vz = rand() % radius;
    ApplyMatrixSV(&m, &in, &out);
    entity->pos.vx += out.vx;
    entity->pos.vy += out.vy;
    entity->pos.vz += out.vz;
}
