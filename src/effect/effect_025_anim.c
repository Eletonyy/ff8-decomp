/**
 * @file effect_025_anim.c
 * @brief Cura: draw an animation frame at the entity, upright or rolled.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/common.h"
#include "effect/effect_025_anim.h"

/** @brief Draw @p anim upright at the entity's position, tinted with @p colour. */
void func_801A4FE0(EffectEntity *entity, BattleSpriteAnim *anim,
                   CVECTOR *colour) {
    MATRIX m;
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
    prim->anim = anim;
    prim->frame = entity->unk050;
    prim->flags = BATTLE_SPRITE_FLAG_COLOUR;
    prim->colour = *colour;
    g_effectPrimCursor = func_800C9E10(prim, D_800FA5E8->ot, 2, g_effectPrimCursor);
    func_800B36B8(sizeof(BattleSpritePrim));
}

/** @brief As @ref func_801A4FE0, rolled by @p angle and scaled by @p scale. */
void func_801A50D4(EffectEntity *entity, BattleSpriteAnim *anim,
                   CVECTOR *colour, s16 angle, s16 scale) {
    MATRIX m;
    VECTOR scaleVec;
    BattleSpritePrim *prim;

    if (entity->flags & EFFECT_FLAG_DONE) {
        return;
    }
    scaleVec.vz = scale;
    scaleVec.vy = scale;
    scaleVec.vx = scale;
    effectMatrixIdentity(&m);
    effectMatrixRotY(&m, angle);
    effectMatrixRotX(&m, 0x400);
    ScaleMatrix(&m, &scaleVec);
    m.t[0] = entity->pos.vx;
    m.t[1] = 0;
    m.t[2] = entity->pos.vz;
    CompMatrix(&D_800F02C8, &m, &m);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    prim = func_800B3698(sizeof(BattleSpritePrim));
    prim->anim = anim;
    prim->frame = entity->unk050;
    prim->flags = BATTLE_SPRITE_FLAG_COLOUR;
    prim->colour = *colour;
    g_effectPrimCursor = func_800C9E10(prim, D_800FA5E8->ot, 2, g_effectPrimCursor);
    func_800B36B8(sizeof(BattleSpritePrim));
}
