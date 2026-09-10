/**
 * @file draw.c
 * @brief Run the steps of the pose list and draw the particles they leave.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/common.h"
#include "effect/lib/pose.h"
#include "effect/lib/particlestep.h"
#include "effect/lib/emitteralloc.h"
#include "effect/lib/list.h"
#include "effect/lib/particle.h"
#include "effect/lib/draw.h"

/** @brief GTE @c FLAG bits 18 and 17: SZ3/OTZ saturated, divide overflow. */
#define BATTLE_SPRITE_GTE_OUT_OF_RANGE ((1 << 18) | (1 << 17))

/** @brief The draw-mode packet an additive sprite set is linked behind. */
#define BATTLE_SPRITE_ADDITIVE_MODE (0xE1000000 | (1 << 9) | getTPage(0, 1, 0, 0))

/** @brief Highest texture coordinate a sprite's right or bottom edge takes. */
#define BATTLE_SPRITE_UV_MAX 0xFF

static void effectStepRunKind(EffectParticle *node, EffectPoseStep *step);
static s32 effectStepFire(EffectParticle *node, EffectPoseStep *step);
static POLY_FT4 *effectSpriteEmit(BattleSpritePrim *prim, u32 *ot, s32 otShift,
                                  POLY_FT4 *head);
static POLY_FT4 *effectSpritePacket(BattleSpritePrim *prim, u32 *ot,
                                    s32 otShift, POLY_FT4 *head);
static void effectParticleDrawPacket(EffectParticle *node, EffectPoseStep *step);

/** @brief Run @p step when it is the kind this handler serves. */
static void effectStepRunKind(EffectParticle *particle, EffectPoseStep *step) {
    if (step->unk030 == 1) {
        effectEmitterAlloc(particle, step->unk031, particle->unk1D9);
    }
}

/**
 * @brief Decide whether @p step fires this frame, and run it if it does.
 * @return 1 when the step ran, 0 otherwise.
 */
static s32 effectStepFire(EffectParticle *particle, EffectPoseStep *step) {
    switch (step->kind) {
    case 0:
        if (particle->unk1D2 != 1) {
            return 0;
        }
        break;
    case 1:
        particle->unk1C8--;
        if (particle->unk1C8 >= 0) {
            return 0;
        }
        effectStepRunKind(particle, step);
        return 1;
    case 2:
        particle->unk1C8--;
        if (particle->unk1C8 < 0) {
            break;
        }
        if (particle->unk1D2 != 1) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    effectStepRunKind(particle, step);
    return 1;
}

/** @brief Step one particle of the pose list, or retire it once its phase is over. */
void effectParticleStep(EffectParticle *particle) {
    EffectPoseStep *step = g_effectDrawList->sources[particle->unk1D6];

    switch (particle->unk1CC) {
    case 0:
        effectParticleAgeHold(particle, step);
        if (effectStepFire(particle, step) == 0) {
            effectParticleAdvance(particle, step);
            effectParticlePose(particle, step);
            particle->unk1CA++;
        } else {
            particle->stream = NULL;
            particle->unk1CC++;
        }
        break;
    case 1:
        effectListRemove(particle);
        particle->inUse = 0;
        g_effectDrawList->live--;
        break;
    }
}

/** @brief One less than @p particle's count, as a signed step. */
s16 effectEmitterCountStep(EffectEmitter *emitter) {
    return emitter->unk008 - 1;
}

/**
 * @brief The overlay's own copy of @ref func_800C97E4: emit the frame's sprites.
 *
 * @return The prim buffer cursor past the packets emitted.
 */
static POLY_FT4 *effectSpriteEmit(BattleSpritePrim *prim, u32 *ot, s32 otShift, POLY_FT4 *head) {
    POLY_FT4 *p = head;
    POLY_FT4 *next;
    BattleSprite *sprite = prim->sprites;
    s32 count = prim->spriteCount;
    s32 i;
    s32 flags;
    s32 value; /* angle, then each scale: one local, the target keeps all three in s0 */
    s32 w;
    s32 h;
    s32 uv;
    u32 shade;
    DR_MODE *tp;

    for (i = 0; i < count; i++, sprite++) {
        prim->m.m[2][1] = 0;
        prim->m.m[2][0] = 0;
        prim->m.m[1][2] = 0;
        prim->m.m[1][0] = 0;
        prim->m.m[0][2] = 0;
        prim->m.m[0][1] = 0;
        prim->m.m[2][2] = ONE;
        flags = sprite->flags;
        if (flags & BATTLE_SPRITE_TRANSFORMED) {
            value = sprite->angle;
            if (value == 0) {
                prim->cos = ONE;
                prim->sin = 0;
                prim->lastAngle = 0;
            } else if (value != prim->lastAngle) {
                prim->cos = rcos(value);
                prim->sin = rsin(value);
                prim->lastAngle = value;
            }
            value = sprite->scaleX;
            prim->m.m[0][0] = prim->cos * value >> 12;
            prim->m.m[1][0] = prim->sin * value >> 12;
            value = sprite->scaleY;
            prim->m.m[0][1] = -prim->sin * value >> 12;
            prim->m.m[1][1] = prim->cos * value >> 12;
        } else {
            prim->m.m[1][1] = ONE;
            prim->m.m[0][0] = ONE;
        }
        w = sprite->w;
        h = sprite->h;
        prim->corners[2].vx = -(w << 3);
        prim->corners[0].vx = -(w << 3);
        prim->corners[3].vx = w << 3;
        prim->corners[1].vx = w << 3;
        prim->corners[1].vy = -(h << 3);
        prim->corners[0].vy = -(h << 3);
        prim->corners[3].vy = h << 3;
        prim->corners[2].vy = h << 3;
        prim->m.t[0] = (sprite->x << 4) + (w << 3);
        prim->m.t[1] = (sprite->y << 4) + (h << 3);
        prim->m.t[2] = 0;
        /* m = mtx * m, a column at a time, with the packet filled in the GTE's shadow. */
        gte_SetRotMatrix(&prim->mtx);
        gte_ldclmv(&prim->m.m[0][0]);
        gte_rtir();
        setlen(p, 9);
        /* One word each: w, shade, h, code and u, v, clut are laid out as the packet's. */
        *(u32 *)&p->r0 = *(u32 *)&sprite->w;
        *(u32 *)&p->u0 = *(u32 *)&sprite->u;
        gte_stclmv(&prim->m.m[0][0]);
        gte_ldclmv(&prim->m.m[0][1]);
        gte_rtir();
        p->r0 = p->b0 = p->g0;
        shade = p->r0 * prim->colour.r;
        p->r0 = shade >> 7;
        gte_stclmv(&prim->m.m[0][1]);
        gte_ldclmv(&prim->m.m[0][2]);
        gte_rtir();
        p->u2 = sprite->u;
        shade = p->g0 * prim->colour.g;
        p->g0 = shade >> 7;
        gte_stclmv(&prim->m.m[0][2]);
        gte_SetTransMatrix(&prim->mtx);
        gte_ldlv0(prim->m.t);
        gte_rt();
        p->v1 = sprite->v;
        shade = p->b0 * prim->colour.b;
        p->b0 = shade >> 7;
        gte_stlvnl(prim->m.t);
        gte_SetRotMatrix(&prim->m);
        gte_SetTransMatrix(&prim->m);
        gte_ldv3(&prim->corners[0], &prim->corners[1], &prim->corners[2]);
        gte_rtpt();
        uv = sprite->u + w - prim->uvInset;
        if (uv > BATTLE_SPRITE_UV_MAX) {
            uv = BATTLE_SPRITE_UV_MAX;
        }
        p->u3 = uv;
        p->u1 = uv;
        /* The row select is the flag word's top nibble, unsigned. */
        p->clut += getClut(0, prim->clutRow[(u16)flags >> BATTLE_SPRITE_CLUT_ROW_SHIFT]);
        gte_stflg(&prim->gteFlag);
        if (!(prim->gteFlag & BATTLE_SPRITE_GTE_OUT_OF_RANGE)) {
            p->tpage = flags & BATTLE_SPRITE_TPAGE_MASK;
            gte_stsxy3(&p->x0, &p->x1, &p->x2);
            gte_ldv0(&prim->corners[3]);
            gte_rtps();
            uv = sprite->v + h - prim->uvInset;
            if (uv > BATTLE_SPRITE_UV_MAX) {
                uv = BATTLE_SPRITE_UV_MAX;
            }
            p->v3 = uv;
            p->v2 = uv;
            gte_stsxy(&p->x3);
            if (prim->otz >= 0) {
                addPrim(&ot[otShift], p);
                p++;
            } else {
                gte_avsz4();
                gte_stotz(&prim->otz);
                otShift = prim->otz >> otShift;
                if (prim->flags & BATTLE_SPRITE_FLAG_ADDITIVE) {
                    next = p + 1;
                    setlen(next, 1);
                    tp = (DR_MODE *)next;
                    next = (POLY_FT4 *)(tp + 1);
                    tp->code[0] = BATTLE_SPRITE_ADDITIVE_MODE;
                    addPrim(&ot[otShift], tp);
                    addPrim(&ot[otShift], p);
                    p = next;
                } else {
                    addPrim(&ot[otShift], p);
                    p++;
                }
            }
        }
    }
    return p;
}

/**
 * @brief The overlay's own copy of @ref func_800C9E10: set a sprite packet up.
 *
 * @return The prim buffer cursor past the sprites emitted.
 */
static POLY_FT4 *effectSpritePacket(BattleSpritePrim *prim, u32 *ot, s32 otShift, POLY_FT4 *head) {
    BattleSpriteAnim *anim = prim->anim;
    u16 *offsets = anim->offsets;
    u8 *frame;

    prim->frameCount = anim->frameCount;
    frame = (u8 *)anim + offsets[prim->frame];
    prim->sprites = (BattleSprite *)frame;
    prim->spriteCount = *(s32 *)frame;
    prim->sprites = (BattleSprite *)((s32 *)prim->sprites + 1);
    prim->nextOffset = offsets[prim->frame + 1];
    if (prim->spriteCount < 0) {
        prim->spriteCount &= ~BATTLE_SPRITE_COUNT_FLAG;
        prim->flags |= BATTLE_SPRITE_FLAG_ADDITIVE;
    }
    prim->corners[3].vz = 0;
    prim->corners[2].vz = 0;
    prim->corners[1].vz = 0;
    prim->corners[0].vz = 0;
    gte_ReadRotMatrix(&prim->mtx);
    if (prim->flags & BATTLE_SPRITE_FLAG_ROTATE) {
        RotMatrixZ(prim->angle - D_800F02A0, &prim->mtx);
    } else if (!(prim->flags & BATTLE_SPRITE_FLAG_NO_ROLL)) {
        RotMatrixZ(-D_800F02A0, &prim->mtx);
    }
    if (prim->flags & BATTLE_SPRITE_FLAG_SCALE) {
        prim->scale.vz = ONE;
        ScaleMatrixL(&prim->mtx, &prim->scale);
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_COLOUR)) {
        *(u32 *)&prim->colour = BATTLE_SPRITE_COLOUR_DEFAULT;
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT0)) {
        prim->clutRow[0] = anim->clutRow[0];
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT1)) {
        prim->clutRow[1] = anim->clutRow[1];
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT2)) {
        prim->clutRow[2] = anim->clutRow[2];
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT3)) {
        prim->clutRow[3] = anim->clutRow[3];
    }
    if (prim->flags & BATTLE_SPRITE_FLAG_UV_INSET) {
        prim->uvInset = 1;
    } else {
        prim->uvInset = 0;
    }
    prim->cos = ONE;
    prim->lastAngle = 0;
    prim->sin = 0;
    prim->otz = -1;
    return effectSpriteEmit(prim, ot, otShift, head);
}

/**
 * @brief Build one draw packet for @p particle's model and link it into the OT.
 *
 * @note @p step is not read; every caller has it to hand and passes it.
 */
static void effectParticleDrawPacket(EffectParticle *particle, EffectPoseStep *step) {
    BattleSpritePrim *prim;

    g_effectStackTop -= sizeof(BattleSpritePrim);
    prim = (BattleSpritePrim *)g_effectStackTop;
    gte_SetRotMatrix(&particle->mtx);
    gte_SetTransMatrix(&particle->mtx);
    prim->anim = particle->stream;
    prim->frame = particle->unk1D0;
    prim->flags = 0;
    g_effectPrimCursor = effectSpritePacket(prim, D_800FA5E8->ot, 2, g_effectPrimCursor);
    g_effectStackTop += sizeof(BattleSpritePrim);
}

/** @brief Draw every particle on the list that has a model and is still posing. */
void effectDrawParticles(void) {
    EffectParticle *particle = g_effectDrawList->head;

    while (particle != NULL) {
        if (particle->unk008 == 1) {
            EffectPoseStep *step = g_effectDrawList->sources[particle->unk1D6];
            EffectParticle *n = particle;

            if (step->mode < 3 && particle->stream != NULL) {
                if (particle->copies == 1) {
                    effectParticleDrawPacket(particle, step);
                } else {
                    s32 i;

                    for (i = 0; i < n->copies; i++) {
                        n->mtx.t[0] = n->offsets[i].vx;
                        n->mtx.t[1] = n->offsets[i].vy;
                        n->mtx.t[2] = n->offsets[i].vz;
                        effectParticleDrawPacket(n, step);
                    }
                }
            }
        }
        particle = particle->next;
    }
}
