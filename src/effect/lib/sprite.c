/**
 * @file sprite.c
 * @brief One sprite through the battle sprite packet.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/common.h"
#include "effect/lib/sprite.h"

/** @brief Build this effect's primitive and link it into the battle display list. */
void effectDrawSprite(EffectEntity *entity) {
    s16 count;
    BattleSpritePrim *prim;

    if (entity->flags & EFFECT_FLAG_DONE) {
        return;
    }
    prim = func_800B3698(sizeof(BattleSpritePrim));
    func_800C96E4(&entity->pos, ONE, entity->unk054);
    prim->anim = entity->unk04C;
    count = entity->unk050;
    prim->flags = 0;
    prim->frame = count;
    g_effectPrimCursor = func_800C9E10(prim, D_800FA5E8->ot, 2, g_effectPrimCursor);
    func_800B36B8(sizeof(BattleSpritePrim));
}
