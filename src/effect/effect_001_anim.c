/**
 * @file effect_001_anim.c
 * @brief Cure: draw an animation frame at the entity.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/common.h"
#include "effect/effect_001_anim.h"

/** @brief Draw @p anim at the entity's position in @p colour and link it into the OT. */
void func_801A172C(EffectEntity *entity, BattleSpriteAnim *anim,
                   CVECTOR *colour) {
    MATRIX m;
    void **head;
    BattleSpritePrim *prim;

    if (entity->flags & EFFECT_FLAG_DONE) {
        return;
    }
    effectMatrixIdentity(&m);
    effectMatrixRotX(&m, 0x400);
    m.t[0] = entity->pos.vx;
    m.t[1] = 0;
    m.t[2] = entity->pos.vz;
    CompMatrix(&D_800F02C8, &m, &m);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    prim = func_800B3698(sizeof(BattleSpritePrim));
    head = &g_effectPrimCursor;
    prim->anim = anim;
    prim->frame = 0;
    prim->flags = BATTLE_SPRITE_FLAG_COLOUR;
    prim->colour = *colour;
    *head = func_800C9E10(prim, D_800FA5E8->ot, 2, *head);
    func_800B36B8(sizeof(BattleSpritePrim));
}
