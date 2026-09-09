/**
 * @file effect_039_entry.c
 * @brief Drain: the overlay entry point.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/tables.h"
#include "effect/effect_039.h"
#include "effect/effect_039_entry.h"

/** @brief Stride between the prim and frame banks carved out of the TIM. */
#define EFFECT_BANK_SIZE 0x6000

/**
 * @brief Start the effect's script and hand back its task pool.
 *
 * @param animSet Animation set the effect plays.
 * @return The pool the root entity lives in.
 */
void *func_801A0000(EffectAnimSet *animSet) {
    u8 *bank0;
    EffectEntity *entity;
    u8 *bank1;
    u8 *frames;

    D_801D9744 = 0;
    D_801D9748 = 0;
    func_800B2A00(&D_801DA304, &D_801DA234, 0x64, 2);
    entity = effectSpawnTask(&D_801DA304, func_801A9D2C, 0x64, NULL);
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
        func_800C3BE0(&D_801B01EC);
        func_800BB084(&D_801B0924);
    }
    D_801D9754 = &D_801B0924;
    bank0 = &D_801B0924;
    D_801DA21C = bank0;
    bank1 = bank0 + EFFECT_BANK_SIZE;
    D_801DA224 = bank0 + EFFECT_BANK_SIZE;
    D_801D9754 = bank0 + EFFECT_BANK_SIZE;
    frames = bank1;
    D_801DA220 = frames;
    frames += EFFECT_BANK_SIZE;
    D_801D9754 = frames;
    D_801DA228 = frames;
    func_800B2A00(&D_801DA474, &D_801DA314, 0x58, 4);
    func_800B2A00(&D_801DA544, &D_801DA484, 0x3C, 3);
    func_800B2A00(&g_effectChildPool, &D_801D9A14, 0x2A4, 3);
    func_800B2A00(&D_801D99E4, &D_801D9764, 0x40, 0xA);
    return &D_801DA304;
}
