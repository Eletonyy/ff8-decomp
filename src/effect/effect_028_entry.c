/**
 * @file effect_028_entry.c
 * @brief Curaga: the overlay entry point.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/tables.h"
#include "effect/effect_028.h"
#include "effect/effect_028_entry.h"

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

    D_801D3CCC = 0;
    D_801D3CD0 = 0;
    func_800B2A00(&D_801D488C, &D_801D47BC, 0x64, 2);
    entity = effectSpawnTask(&D_801D488C, func_801A9CF4, 0x64, NULL);
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
        func_800C3BE0(&D_801AA10C);
        func_800BB084(&D_801AACAC);
    }
    D_801D3CDC = &D_801AACAC;
    bank0 = &D_801AACAC;
    D_801D47A4 = bank0;
    bank1 = bank0 + EFFECT_BANK_SIZE;
    D_801D47AC = bank0 + EFFECT_BANK_SIZE;
    D_801D3CDC = bank0 + EFFECT_BANK_SIZE;
    frames = bank1;
    D_801D47A8 = frames;
    frames += EFFECT_BANK_SIZE;
    D_801D3CDC = frames;
    D_801D47B0 = frames;
    func_800B2A00(&D_801D49FC, &D_801D489C, 0x58, 4);
    func_800B2A00(&D_801D4ACC, &D_801D4A0C, 0x3C, 3);
    func_800B2A00(&g_effectChildPool, &D_801D3F9C, 0x2A4, 3);
    func_800B2A00(&D_801D3F6C, &D_801D3CEC, 0x40, 0xA);
    return &D_801D488C;
}
