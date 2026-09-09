/**
 * @file spawn.c
 * @brief Spawn particles off emitters and place emitters on the points their sources name.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/libc.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/common.h"
#include "effect/lib/rotz.h"
#include "effect/lib/pose.h"
#include "effect/lib/vector.h"
#include "effect/lib/particlealloc.h"
#include "effect/lib/draw.h"
#include "effect/lib/list.h"
#include "effect/lib/particle.h"
#include "effect/lib/spawn.h"

/** @brief The scratch a spawned particle's aim and speed are worked out in. */
typedef struct {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ SVECTOR dir;    /**< Straight down, then turned by @c m. */
    /* 0x28 */ SVECTOR angle;  /**< The turn @c m was built from. */
    /* 0x30 */ SVECTOR spin;   /**< The turn the particle's own pose takes. */
    /* 0x38 */ VECTOR vel;
    /* 0x48 */ s16 speedX;
    /* 0x4A */ s16 speedY;
    /* 0x4C */ s16 speedZ;
    /* 0x4E */ s16 frame;      /**< The mote's age, which indexes the tables. */
} EffectSpawnScratch; /* 0x50 */

static void effectEmitterAge(EffectEmitter *node);
static void effectEmitterSpawn(EffectEmitter *mote, EffectPoseStep *step, s32 copies);
static void effectEmitterSpawnShare(EffectEmitter *node);

/** @brief Step @p emitter's life and retire it when it runs out. */
static void effectEmitterAge(EffectEmitter *emitter) {
    emitter->life--;
    if (emitter->life <= 0) {
        effectListRemove((EffectParticle *)emitter);
        emitter->inUse = 0;
        g_effectDrawList->count--;
    }
}

/** @brief Spawn a particle off @p emitter and give each of its @p copies its own aim. */
static void effectEmitterSpawn(EffectEmitter *emitter, EffectPoseStep *step, s32 copies) {
    EffectSpawnScratch *s;
    EffectParticle *particle;
    s32 i;

    g_effectStackTop -= sizeof(EffectSpawnScratch);
    s = (EffectSpawnScratch *)g_effectStackTop;
    particle = effectParticleAlloc(emitter, emitter->unk06A);
    particle->copies = copies;
    s->frame = emitter->unk060;
    if (step->mode < 3) {
        particle->stream = g_effectDrawList->unk228[emitter->unk06A];
        particle->unk1D1 = effectEmitterCountStep(particle->stream);
        if (step->mode == 2) {
            particle->unk1D3 = step->unk017;
            particle->unk1D4 = step->unk018;
            particle->unk1D5 = effectRandBetween(step->unk019, step->unk01A);
        }
    }
    particle->unk1DA = step->unk064;
    for (i = 0; i < copies; i++) {
        particle->pos[i] = emitter->pos;
        if (step->unk022 == 0) {
            particle->offset[i] = emitter->pos;
        }
        if (step->unk034 == 1) {
            s->angle.vx = rand() & 0xFFF;
            s->angle.vy = rand() & 0xFFF;
            s->angle.vz = rand() & 0xFFF;
            effectMatrixIdentity(&s->m);
            if (s->angle.vz != 0) {
                effectMatrixRotZ(&s->m, s->angle.vz);
            }
            if (s->angle.vx != 0) {
                effectMatrixRotX(&s->m, s->angle.vx);
            }
            if (s->angle.vy != 0) {
                effectMatrixRotY(&s->m, s->angle.vy);
            }
            if (step->unk033 == 0) {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
                if (s->speedX != 0) {
                    s->speedX = rand() % s->speedX;
                }
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                if (s->speedZ != 0) {
                    s->speedZ = rand() % s->speedZ;
                }
            } else {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
            }
            s->dir.vx = 0;
            s->dir.vy = -0x1000;
            s->dir.vz = 0;
            gte_SetRotMatrix(&s->m);
            gte_ldv0(&s->dir);
            gte_mvmva(1, 0, 0, 3, 0);
            gte_stsv(&s->dir);
            s->vel.vx = s->dir.vx * s->speedX << 4;
            s->vel.vy = s->dir.vy * s->speedY << 4;
            s->vel.vz = s->dir.vz * s->speedZ << 4;
            particle->offset[i].vx += s->vel.vx;
            particle->offset[i].vy += s->vel.vy;
            particle->offset[i].vz += s->vel.vz;
        } else if (step->unk034 == 2) {
            s->angle.vx = rand() & 0xFFF;
            s->angle.vy = 0;
            s->angle.vz = 0x400;
            effectMatrixIdentity(&s->m);
            if (s->angle.vz != 0) {
                effectMatrixRotZ(&s->m, s->angle.vz);
            }
            if (s->angle.vx != 0) {
                effectMatrixRotX(&s->m, s->angle.vx);
            }
            if (s->angle.vy != 0) {
                effectMatrixRotY(&s->m, s->angle.vy);
            }
            if (step->unk033 == 0) {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
                if (s->speedX != 0) {
                    s->speedX = rand() % s->speedX;
                }
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                if (s->speedZ != 0) {
                    s->speedZ = rand() % s->speedZ;
                }
            } else if (step->unk033 != 1) {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
                if (s->speedX != 0) {
                    s->speedX = rand() % s->speedX;
                }
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                if (s->speedZ != 0) {
                    s->speedZ = rand() % s->speedZ;
                }
            } else {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                s->speedZ = step->unk0D4[s->frame];
            }
            if (rand() & 1) {
                s32 speed = s->speedY;

                s->speedY = -speed;
            }
            s->dir.vx = 0;
            s->dir.vy = -0x1000;
            s->dir.vz = 0;
            gte_SetRotMatrix(&s->m);
            gte_ldv0(&s->dir);
            gte_mvmva(1, 0, 0, 3, 0);
            gte_stsv(&s->dir);
            s->vel.vx = s->dir.vx * s->speedX << 4;
            s->vel.vy = s->speedY << 16;
            s->vel.vz = s->dir.vz * s->speedZ << 4;
            particle->offset[i].vx += s->vel.vx;
            particle->offset[i].vy += s->vel.vy;
            particle->offset[i].vz += s->vel.vz;
        }
        switch (step->unk021) {
        case 0:
            effectJitterSVector(&s->spin, &step->unk03C, &step->unk044);
            break;
        case 1:
            s->spin = s->angle;
            break;
        }
        effectMatrixIdentity(&particle->poses[i]);
        if (s->spin.vz != 0) {
            effectMatrixRotZ(&particle->poses[i], s->spin.vz);
        }
        if (s->spin.vx != 0) {
            effectMatrixRotX(&particle->poses[i], s->spin.vx);
        }
        if (s->spin.vy != 0) {
            effectMatrixRotY(&particle->poses[i], s->spin.vy);
        }
        gte_MulMatrix0(&emitter->mtx, &particle->poses[i], &particle->poses[i]);
        if (step->unk027 == 0) {
            particle->spin[i] = effectRandProduct(step->unk04C, step->unk050);
            particle->spinRate[i] = effectRandProduct(step->unk054, step->unk058);
        }
    }
    effectJitterSVector(&particle->angleBase, &step->unk148, &step->unk150);
    if (step->kind == 1 || step->kind == 2) {
        particle->unk1C8 = effectRandBetween(step->unk01F, step->unk020);
    }
    if (step->unk027 == 0) {
        particle->unk1DC = effectRandProduct(step->unk05C, step->unk060);
        for (i = 0; i < copies; i++) {
            effectRandVector(&particle->vel[i], &step->unk068, &step->unk078);
            effectRandVector(&particle->accel[i], &step->unk088, &step->unk098);
            effectRandVector(&particle->jerk[i], &step->unk0A8, &step->unk0B8);
        }
    }
    effectStepFireSlots(particle, step);
    g_effectStackTop += sizeof(EffectSpawnScratch);
}

/** @brief Emit this frame's share of @p emitter's work, in chunks its source sizes. */
static void effectEmitterSpawnShare(EffectEmitter *emitter) {
    EffectPoseStep *step = g_effectDrawList->sources[emitter->unk06A];
    s32 age = emitter->unk060;
    s32 want;
    s32 full;
    s32 rest;
    s32 i;

    if (age < 0x28) {
        want = step->unk0C8[age];
        if (want != 0) {
            full = want / step->unk036;
            rest = want % step->unk036;
            for (i = 0; i < full; i++) {
                effectEmitterSpawn(emitter, step, step->unk036);
            }
            if (rest > 0) {
                effectEmitterSpawn(emitter, step, rest);
            }
        }
    }
}

/** @brief Step one emitter: set it up once, run its kind, then age it. */
void effectEmitterStep(EffectEmitter *emitter) {
    if (emitter->unk062 == 0) {
        /* Called with no argument: the mote is already in $a0 and stays there. */
        ((void (*)())effectEmitterSetup)();
        emitter->unk062++;
    }
    switch (emitter->kind) {
    case 3:
        effectEmitterOnOwner(emitter);
        break;
    case 4:
        effectEmitterSlide(emitter);
        break;
    }
    effectEmitterSpawnShare(emitter);
    emitter->unk060++;
    effectEmitterAge(emitter);
}

/** @brief Put @p emitter on the strand point its source names, plus that source's offset. */
void effectEmitterOnStrand(EffectEmitter *emitter) {
    EffectPoseStep *step = g_effectDrawList->sources[emitter->unk06A];
    SVECTOR offset;
    SVECTOR turned;
    MATRIX m;

    switch (step->anchor) {
    case 0:
        emitter->pos = g_effectDrawList->strands[1][emitter->unk06B];
        break;
    case 1:
        emitter->pos = g_effectDrawList->strands[2][emitter->unk06B];
        break;
    case 2:
        emitter->pos = g_effectDrawList->strands[3][emitter->unk06B];
        break;
    case 3:
        emitter->pos = g_effectDrawList->strands[4][emitter->unk06B];
        break;
    }
    offset.vx = step->offsetX / 0x10000;
    offset.vy = step->offsetY / 0x10000;
    offset.vz = step->offsetZ / 0x10000;
    m = emitter->mtx;
    gte_SetRotMatrix(&m);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    emitter->pos.vx += turned.vx << 16;
    emitter->pos.vy += turned.vy << 16;
    emitter->pos.vz += turned.vz << 16;
}

/** @brief Put @p emitter on the trail point its source names, plus that source's offset. */
void effectEmitterOnTrail(EffectEmitter *emitter) {
    EffectPoseStep *step = g_effectDrawList->sources[emitter->unk06A];
    SVECTOR offset;
    SVECTOR turned;

    switch (step->anchor) {
    case 0:
        emitter->pos = g_effectDrawList->trail[2];
        break;
    case 1:
        emitter->pos = g_effectDrawList->trail[3];
        break;
    case 2:
        emitter->pos = g_effectDrawList->trail[4];
        break;
    case 3:
        emitter->pos = g_effectDrawList->trail[5];
        break;
    }
    offset.vx = step->offsetX / 0x10000;
    offset.vy = step->offsetY / 0x10000;
    offset.vz = step->offsetZ / 0x10000;
    gte_SetRotMatrix(&emitter->mtx);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    emitter->pos.vx += turned.vx << 16;
    emitter->pos.vy += turned.vy << 16;
    emitter->pos.vz += turned.vz << 16;
}

/** @brief Put @p emitter on the node that owns it, plus that source's turned offset. */
void effectEmitterOnOwner(EffectEmitter *emitter) {
    EffectPoseStep *step = g_effectDrawList->sources[emitter->unk06A];
    EffectParticle *owner = emitter->owner;

    if (owner != NULL) {
        SVECTOR offset;
        SVECTOR turned;

        if (step->midpoint == 1) {
            emitter->pos.vx = (owner->pos[0].vx + owner->lastPos.vx) / 2;
            emitter->pos.vy = (owner->pos[0].vy + owner->lastPos.vy) / 2;
            emitter->pos.vz = (owner->pos[0].vz + owner->lastPos.vz) / 2;
        } else {
            emitter->pos = owner->pos[0];
        }
        if (step->unk01C == 3) {
            emitter->mtx = owner->orient;
            offset.vx = step->offsetX / 0x10000;
            offset.vy = step->offsetY / 0x10000;
            offset.vz = step->offsetZ / 0x10000;
            gte_SetRotMatrix(&emitter->mtx);
            gte_ldv0(&offset);
            gte_mvmva(1, 0, 0, 3, 0);
            gte_stsv(&turned);
            emitter->pos.vx += turned.vx << 16;
            emitter->pos.vy += turned.vy << 16;
            emitter->pos.vz += turned.vz << 16;
        }
    }
}

/** @brief Slide @p emitter between the two points its source names. */
void effectEmitterSlide(EffectEmitter *emitter) {
    EffectPoseStep *step = g_effectDrawList->sources[emitter->unk06A];
    u8 kind;
    s32 index;
    SVECTOR from;
    SVECTOR to;
    SVECTOR offset;
    SVECTOR turned;
    s32 t;

    kind = step->unk100[emitter->unk060];
    index = step->unk108[emitter->unk060];
    switch (kind) {
    case 0:
        switch (index) {
        case 0:
            from.vx = g_effectDrawList->strands[1][emitter->unk06B].vx / 0x10000;
            from.vy = g_effectDrawList->strands[1][emitter->unk06B].vy / 0x10000;
            from.vz = g_effectDrawList->strands[1][emitter->unk06B].vz / 0x10000;
            break;
        case 1:
            from.vx = g_effectDrawList->strands[2][emitter->unk06B].vx / 0x10000;
            from.vy = g_effectDrawList->strands[2][emitter->unk06B].vy / 0x10000;
            from.vz = g_effectDrawList->strands[2][emitter->unk06B].vz / 0x10000;
            break;
        case 2:
            from.vx = g_effectDrawList->strands[3][emitter->unk06B].vx / 0x10000;
            from.vy = g_effectDrawList->strands[3][emitter->unk06B].vy / 0x10000;
            from.vz = g_effectDrawList->strands[3][emitter->unk06B].vz / 0x10000;
            break;
        case 3:
            from.vx = g_effectDrawList->strands[4][emitter->unk06B].vx / 0x10000;
            from.vy = g_effectDrawList->strands[4][emitter->unk06B].vy / 0x10000;
            from.vz = g_effectDrawList->strands[4][emitter->unk06B].vz / 0x10000;
            break;
        }
        break;
    case 1:
        switch (index) {
        case 0:
            from.vx = g_effectDrawList->trail[2].vx / 0x10000;
            from.vy = g_effectDrawList->trail[2].vy / 0x10000;
            from.vz = g_effectDrawList->trail[2].vz / 0x10000;
            break;
        case 1:
            from.vx = g_effectDrawList->trail[3].vx / 0x10000;
            from.vy = g_effectDrawList->trail[3].vy / 0x10000;
            from.vz = g_effectDrawList->trail[3].vz / 0x10000;
            break;
        case 2:
            from.vx = g_effectDrawList->trail[4].vx / 0x10000;
            from.vy = g_effectDrawList->trail[4].vy / 0x10000;
            from.vz = g_effectDrawList->trail[4].vz / 0x10000;
            break;
        case 3:
            from.vx = g_effectDrawList->trail[5].vx / 0x10000;
            from.vy = g_effectDrawList->trail[5].vy / 0x10000;
            from.vz = g_effectDrawList->trail[5].vz / 0x10000;
            break;
        }
        break;
    }
    offset.vx = step->unk110[emitter->unk060];
    offset.vy = step->unk114[emitter->unk060];
    offset.vz = step->unk118[emitter->unk060];
    gte_SetRotMatrix(&emitter->mtx);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    from.vx += turned.vx;
    from.vy += turned.vy;
    from.vz += turned.vz;
    kind = step->unk104[emitter->unk060];
    index = step->unk10C[emitter->unk060];
    switch (kind) {
    case 0:
        switch (index) {
        case 0:
            to.vx = g_effectDrawList->strands[1][emitter->unk06B].vx / 0x10000;
            to.vy = g_effectDrawList->strands[1][emitter->unk06B].vy / 0x10000;
            to.vz = g_effectDrawList->strands[1][emitter->unk06B].vz / 0x10000;
            break;
        case 1:
            to.vx = g_effectDrawList->strands[2][emitter->unk06B].vx / 0x10000;
            to.vy = g_effectDrawList->strands[2][emitter->unk06B].vy / 0x10000;
            to.vz = g_effectDrawList->strands[2][emitter->unk06B].vz / 0x10000;
            break;
        case 2:
            to.vx = g_effectDrawList->strands[3][emitter->unk06B].vx / 0x10000;
            to.vy = g_effectDrawList->strands[3][emitter->unk06B].vy / 0x10000;
            to.vz = g_effectDrawList->strands[3][emitter->unk06B].vz / 0x10000;
            break;
        case 3:
            to.vx = g_effectDrawList->strands[4][emitter->unk06B].vx / 0x10000;
            to.vy = g_effectDrawList->strands[4][emitter->unk06B].vy / 0x10000;
            to.vz = g_effectDrawList->strands[4][emitter->unk06B].vz / 0x10000;
            break;
        }
        break;
    case 1:
        switch (index) {
        case 0:
            to.vx = g_effectDrawList->trail[2].vx / 0x10000;
            to.vy = g_effectDrawList->trail[2].vy / 0x10000;
            to.vz = g_effectDrawList->trail[2].vz / 0x10000;
            break;
        case 1:
            to.vx = g_effectDrawList->trail[3].vx / 0x10000;
            to.vy = g_effectDrawList->trail[3].vy / 0x10000;
            to.vz = g_effectDrawList->trail[3].vz / 0x10000;
            break;
        case 2:
            to.vx = g_effectDrawList->trail[4].vx / 0x10000;
            to.vy = g_effectDrawList->trail[4].vy / 0x10000;
            to.vz = g_effectDrawList->trail[4].vz / 0x10000;
            break;
        case 3:
            to.vx = g_effectDrawList->trail[5].vx / 0x10000;
            to.vy = g_effectDrawList->trail[5].vy / 0x10000;
            to.vz = g_effectDrawList->trail[5].vz / 0x10000;
            break;
        }
        break;
    }
    offset.vx = step->unk11C[emitter->unk060];
    offset.vy = step->unk120[emitter->unk060];
    offset.vz = step->unk124[emitter->unk060];
    gte_SetRotMatrix(&emitter->mtx);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    to.vx += turned.vx;
    to.vy += turned.vy;
    to.vz += turned.vz;
    t = step->unk128[emitter->unk060];
    emitter->pos.vx = (from.vx << 16) + ((to.vx - from.vx) * t << 4);
    emitter->pos.vy = (from.vy << 16) + ((to.vy - from.vy) * t << 4);
    emitter->pos.vz = (from.vz << 16) + ((to.vz - from.vz) * t << 4);
}
