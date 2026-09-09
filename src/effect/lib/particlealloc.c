/**
 * @file particlealloc.c
 * @brief Allocate a particle from its fixed pool of slots.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/list.h"
#include "effect/lib/particle.h"
#include "effect_params.h"
#include "effect/lib/particlealloc.h"

/**
 * @brief Allocate a particle from its pool and put it on the draw list.
 *
 * The search starts where the last one left off, and tries one slot more
 * than the ring holds, so the slot it started on is examined last.
 *
 * @return The particle, or NULL when the pool is full.
 */
EffectParticle *effectParticleAlloc(EffectEmitter *owner, u8 source) {
    EffectParticle *found = NULL;
    s32 i = g_effectParticleCursor;
    s32 n;

    for (n = 0; n < EFFECT_PARTICLE_COUNT + 1; n++) {
        if (g_effectParticleBank[i].inUse == 0) {
            EffectParticle *particle = &g_effectParticleBank[i];

            found = particle;
            effectZeroWords(particle, sizeof(EffectParticle));
            particle->inUse = 1;
            particle->unk1D6 = source;
            particle->owner = owner;
            particle->unk1D9 = owner->unk06B;
            g_effectDrawList->live++;
            effectListAppend(particle, 1);
            break;
        }
        i++;
        if (i >= EFFECT_PARTICLE_COUNT) {
            i = 0;
        }
    }
    i++;
    if (i >= EFFECT_PARTICLE_COUNT) {
        i = 0;
    }
    g_effectParticleCursor = i;
    return found;
}
