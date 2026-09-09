/**
 * @file emitteralloc.c
 * @brief Allocate an emitter from its fixed pool of slots.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/pose.h"
#include "effect/lib/list.h"
#include "effect/lib/particle.h"
#include "effect_params.h"
#include "effect/lib/emitteralloc.h"

/**
 * @brief Allocate an emitter from its pool and put it on the draw list.
 *
 * The search starts where the last one left off and gives up after one lap.
 *
 * @return The emitter, or NULL when the pool is full.
 */
EffectEmitter *effectEmitterAlloc(void *owner, s16 source, s16 index) {
    EffectEmitter *found = NULL;
    s32 i = g_effectEmitterCursor;
    s32 n;

    for (n = 0; n < 0x5A; n++) {
        if (g_effectEmitterBank[i].inUse == 0) {
            EffectEmitter *emitter = &g_effectEmitterBank[i];

            found = emitter;
            effectZeroWords(emitter, sizeof(EffectEmitter));
            emitter->inUse = 1;
            emitter->unk06A = source;
            emitter->unk06B = index;
            emitter->owner = owner;
            g_effectDrawList->count++;
            effectListAppend((EffectParticle *)emitter, 0);
            break;
        }
        i++;
        if (i >= EFFECT_EMITTER_COUNT) {
            i = 0;
        }
    }
    i++;
    if (i >= EFFECT_EMITTER_COUNT) {
        i = 0;
    }
    g_effectEmitterCursor = i;
    return found;
}
