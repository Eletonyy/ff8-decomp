/**
 * @file bankclear.c
 * @brief Clear both record banks and the ring cursors.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/pose.h"
#include "effect/lib/particle.h"
#include "effect_params.h"
#include "effect/lib/bankclear.h"

/** @brief Clear both record banks and the script's frame counters. */
void effectBanksClear(void) {
    effectZeroWords(g_effectEmitterBank, EFFECT_EMITTER_BANK_BYTES);
    effectZeroWords(g_effectParticleBank, EFFECT_PARTICLE_BANK_BYTES);
    g_effectEmitterCursor = 0;
    g_effectParticleCursor = 0;
    g_effectBankUnk90 = 0;
    g_effectBankUnk92 = 0;
}
