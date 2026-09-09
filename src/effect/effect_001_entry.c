/**
 * @file effect_001_entry.c
 * @brief Cure: the overlay entry point.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/effect_001.h"
#include "effect/effect_001_entry.h"

/** @brief Stride between the prim and frame banks carved out of the TIM. */
#define EFFECT_BANK_SIZE 0x10000

/**
 * @brief Start an effect script and hand back its task pool.
 *
 * Builds the four task pools, allocates the root entity, seeds it from the
 * animation set, and publishes the three frame banks the renderer reads from.
 *
 * @param animSet Animation set the effect plays.
 * @return The pool the root entity lives in.
 */
void *func_801A0000(EffectAnimSet *animSet) {
    u8 *bank0;
    EffectEntity *entity;
    u8 *bank1;
    u8 *frames;

    D_801C58E0 = 0;
    D_801C58E4 = 0;
    func_800B2A00(&D_801C59E0, &D_801C5910, 0x64, 2);
    entity = effectSpawnTask(&D_801C59E0, func_801A4434, 0x64, NULL);
    entity->animSet = animSet;
    entity->unk02C = animSet->anims[entity->unk02A].unk000;
    entity->unk02D =
        entity->animSet->anims[entity->unk02A].parts[entity->unk02B].unk000;
    entity->pc = 0;
    entity->unk05A = animSet->anims->unk010;
    entity->unk058 = animSet->anims->unk011;
    entity->unk02F = entity->unk05A - 1;
    if (entity->unk02F < entity->unk058) {
        entity->unk02F = entity->unk058;
    }
    if (!(animSet->flags & EFFECT_ANIMSET_FLAG_LOADED)) {
        func_800C3BE0(&D_801A475C);
        func_800BB084(&D_801A4F0C);
    }
    D_801C58F0 = &D_801A4F0C;
    bank0 = &D_801A4F0C;
    D_801C58FC = bank0;
    bank1 = bank0 + EFFECT_BANK_SIZE;
    D_801C5904 = bank0 + EFFECT_BANK_SIZE;
    D_801C58F0 = bank0 + EFFECT_BANK_SIZE;
    frames = bank1;
    D_801C5900 = frames;
    frames += EFFECT_BANK_SIZE;
    D_801C58F0 = frames;
    D_801C5908 = frames;
    func_800B2A00(&D_801C5B50, &D_801C59F0, 0x58, 4);
    func_800B2A00(&D_801CD9F0, &D_801C5B60, 0x6C, 0x12C);
    func_800B2A00(&D_801D5CF0, &D_801D3900, 0x5C, 0x64);
    return &D_801C59E0;
}
