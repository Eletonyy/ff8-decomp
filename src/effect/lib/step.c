/**
 * @file step.c
 * @brief Script counter stepping.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/step.h"

/** @brief Advance the script's counter, clamping at its limit. */
s32 effectStepCounter(EffectEntity *entity) {
    entity->unk050++;
    if (entity->unk052 < entity->unk050) {
        entity->unk050 = entity->unk052;
        entity->flags |= EFFECT_FLAG_DONE;
        return 1;
    }
    return 0;
}
