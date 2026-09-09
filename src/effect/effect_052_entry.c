/**
 * @file effect_052_entry.c
 * @brief X-Potion: the overlay entry point.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/tables.h"
#include "effect/effect_052.h"
#include "effect/effect_052_entry.h"

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

    D_801D3994 = 0;
    D_801D3998 = 0;
    func_800B2A00(&D_801D42B4, &D_801D41E4, 0x64, 2);
    entity = effectSpawnTask(&D_801D42B4, func_801A99EC, 0x64, NULL);
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
        func_800C3BE0(&D_801A9DD4);
        func_800BB084(&D_801AA974);
    }
    D_801D39A4 = &D_801AA974;
    bank0 = &D_801AA974;
    D_801D41CC = bank0;
    bank1 = bank0 + EFFECT_BANK_SIZE;
    D_801D41D4 = bank0 + EFFECT_BANK_SIZE;
    D_801D39A4 = bank0 + EFFECT_BANK_SIZE;
    frames = bank1;
    D_801D41D0 = frames;
    frames += EFFECT_BANK_SIZE;
    D_801D39A4 = frames;
    D_801D41D8 = frames;
    func_800B2A00(&D_801D4424, &D_801D42C4, 0x58, 4);
    func_800B2A00(&D_801D44F4, &D_801D4434, 0x3C, 3);
    func_800B2A00(&g_effectChildPool, &D_801D39C4, 0x2A4, 3);
    return &D_801D42B4;
}
