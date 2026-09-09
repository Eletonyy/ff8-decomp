/**
 * @file effect_036_entry.c
 * @brief Confuse: the overlay entry point.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/tables.h"
#include "effect/effect_036.h"
#include "effect/effect_036_entry.h"

/** @brief Stride between the prim and frame banks carved out of the TIM. */
#define EFFECT_BANK_SIZE 0xE000

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

    D_801E22F0 = 0;
    D_801E22F4 = 0;
    func_800B2A00(&D_801E2EB0, &D_801E2DE0, 0x64, 2);
    entity = effectSpawnTask(&D_801E2EB0, func_801A9B64, 0x64, NULL);
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
        func_800C3BE0(&D_801B89C8);
        func_800BB084(&D_801B96D0);
    }
    D_801E2300 = &D_801B96D0;
    bank0 = &D_801B96D0;
    D_801E2DC8 = bank0;
    bank1 = bank0 + EFFECT_BANK_SIZE;
    D_801E2DD0 = bank0 + EFFECT_BANK_SIZE;
    D_801E2300 = bank0 + EFFECT_BANK_SIZE;
    frames = bank1;
    D_801E2DCC = frames;
    frames += EFFECT_BANK_SIZE;
    D_801E2300 = frames;
    D_801E2DD4 = frames;
    func_800B2A00(&D_801E3020, &D_801E2EC0, 0x58, 4);
    func_800B2A00(&D_801E30F0, &D_801E3030, 0x3C, 3);
    func_800B2A00(&g_effectChildPool, &D_801E25C0, 0x2A4, 3);
    func_800B2A00(&D_801E2590, &D_801E2310, 0x40, 0xA);
    return &D_801E2EB0;
}
