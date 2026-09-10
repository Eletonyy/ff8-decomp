/**
 * @file packet.c
 * @brief Build a mesh packet for a particle and link its copies into the OT.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/common.h"
#include "effect/lib/pose.h"
#include "effect/lib/emit.h"
#include "effect/lib/particle.h"
#include "effect/lib/packet.h"

/** @brief Build one mesh packet for @p particle and link every copy into the OT. */
void effectBuildMeshPacket(EffectParticle *particle, EffectPoseStep *step) {
    EffectPrimBuild *prim;

    g_effectStackTop -= sizeof(EffectPrimBuild);
    prim = (EffectPrimBuild *)g_effectStackTop;
    prim->stream = particle->stream;
    prim->flags = 0;
    if (step->unk03A == 0) {
        prim->flags = EFFECT_EMIT_TWO_SIDED | EFFECT_EMIT_G_TWO_SIDED;
    }
    if (particle->unk1CE != 0) {
        prim->color = particle->color;
        prim->flags |= EFFECT_EMIT_DEPTH_CUE | EFFECT_EMIT_G_DEPTH_CUE;
        prim->depth = particle->unk1CE;
    }
    if (particle->copies == 1) {

        gte_SetRotMatrix(&particle->mtx);
        gte_SetTransMatrix(&particle->mtx);
        g_effectPrimCursor = effectEmitStream(prim, D_800FA5E8->ot, 2, g_effectPrimCursor);
    } else {
        s32 i;

        for (i = 0; i < particle->copies; i++) {
            particle->mtx.t[0] = particle->offsets[i].vx;
            particle->mtx.t[1] = particle->offsets[i].vy;
            particle->mtx.t[2] = particle->offsets[i].vz;
            gte_SetRotMatrix(&particle->mtx);
            gte_SetTransMatrix(&particle->mtx);
            g_effectPrimCursor = effectEmitStream(prim, D_800FA5E8->ot, 2, g_effectPrimCursor);
        }
    }
    g_effectStackTop += sizeof(EffectPrimBuild);
}
