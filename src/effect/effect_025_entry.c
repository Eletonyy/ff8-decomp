/**
 * @file effect_025_entry.c
 * @brief Cura: the overlay entry point.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/effect_025.h"
#include "effect/effect_025_entry.h"

/**
 * @brief Start the effect's script and hand back its task pool.
 *
 * Builds the four task pools, allocates the root entity, seeds it from the
 * animation set, and publishes the two prim banks the renderer draws through.
 *
 * @param animSet Animation set the effect plays.
 * @return The pool the root entity lives in.
 */
void *func_801A0000(EffectAnimSet *animSet) {
    EffectEntity *entity;
    u8 slot;

    slot = animSet->anims->parts->unk000;
    func_800B2A00(&D_801D3F94, &D_801D3EC4, 0x64, 2);
    entity = effectSpawnTask(&D_801D3F94, func_801A81D0, 0x64, NULL);
    entity->animSet = animSet;
    entity->unk02D = slot;
    entity->unk05A = animSet->anims->unk010;
    entity->unk058 = animSet->anims->unk011;
    if (!(animSet->flags & EFFECT_ANIMSET_FLAG_LOADED)) {
        func_800C3BE0(&D_801A8E50);
        func_800BB084(&D_801A9698);
    }
    D_801D3EBC = &D_801A9698;
    D_801D3EC0 = &D_801BB698;
    func_800B2A00(&D_801D4104, &D_801D3FA4, 0x58, 4);
    func_800B2A00(&D_801D7314, &D_801D4114, 0xC8, 0x40);
    func_800B2A00(&D_801E74F4, &D_801D7324, 0x84, 0x1F4);
    return &D_801D3F94;
}
