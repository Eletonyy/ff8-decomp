/**
 * @file particlestep.c
 * @brief Advance, age and pose the copies of a particle.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/common.h"
#include "effect/lib/rotz.h"
#include "effect/lib/heading.h"
#include "effect/lib/pose.h"
#include "effect/lib/vector.h"
#include "effect/lib/packet.h"
#include "effect/lib/particle.h"
#include "effect/lib/particlestep.h"

/** @brief Run the late step on every particle whose source is in the right mode. */
void effectParticleLateStep(void) {
    EffectParticle *particle = g_effectDrawList->head;

    while (particle != NULL) {
        if (particle->unk008 == 1) {
            s32 idx = particle->unk1D6;
            EffectPoseStep *step = g_effectDrawList->sources[idx];

            if ((u32)(step->mode - 4) < 2 && particle->stream != NULL) {
                effectBuildMeshPacket(particle, step);
            }
        }
        particle = particle->next;
    }
}

/** @brief Advance every copy of @p particle: spin it, drift it and turn its pose. */
void effectParticleAdvance(EffectParticle *particle, EffectPoseStep *step) {
    VECTOR posed[4];
    VECTOR spinOffset;
    VECTOR spun;
    MATRIX m;
    s32 i;

    particle->angle = particle->angleBase;
    particle->angle.vx += step->unk0D8[particle->unk1CA];
    particle->angle.vy += step->unk0DC[particle->unk1CA];
    particle->angle.vz += step->unk0E0[particle->unk1CA];
    particle->angle.vx &= 0xFFF;
    particle->angle.vy &= 0xFFF;
    particle->angle.vz &= 0xFFF;
    particle->lastPos = particle->pos[0];
    switch (step->unk027) {
    case 0:
        for (i = 0; i < particle->copies; i++) {
            if (particle->spinRate != NULL || particle->spin != NULL) {
                particle->spin[i] += particle->spinRate[i];
                if (particle->unk1DA != 0) {
                    effectFadeValue(particle->unk1DA, &particle->spin[i]);
                }
                spinOffset.vx = 0;
                spinOffset.vy = -particle->spin[i];
                spinOffset.vz = 0;
                ApplyMatrixLV(&particle->poses[i], &spinOffset, &spun);
                particle->offset[i].vx += spun.vx;
                particle->offset[i].vy += spun.vy;
                particle->offset[i].vz += spun.vz;
            }
            particle->accel[i].vx += particle->jerk[i].vx;
            particle->accel[i].vy += particle->jerk[i].vy;
            particle->accel[i].vz += particle->jerk[i].vz;
            particle->vel[i].vx += particle->accel[i].vx;
            particle->vel[i].vy += particle->accel[i].vy;
            particle->vel[i].vz += particle->accel[i].vz;
            particle->vel[i].vy += particle->unk1DC;
            if (particle->unk1DA != 0) {
                effectFadeVector(particle->unk1DA, &particle->vel[i]);
            }
            particle->offset[i].vx += particle->vel[i].vx;
            particle->offset[i].vy += particle->vel[i].vy;
            particle->offset[i].vz += particle->vel[i].vz;
        }
        break;
    case 1:
        for (i = 0; i < particle->copies; i++) {
            particle->offset[i].vx = step->unk13C[particle->unk1CA] << 16;
            particle->offset[i].vy = step->unk140[particle->unk1CA] << 16;
            particle->offset[i].vz = step->unk144[particle->unk1CA] << 16;
            if (particle->owner != NULL) {
                m = particle->owner->mtx;
            }
            effectMatrixIdentity(&m);
            ApplyMatrixLV(&m, &particle->offset[i], &particle->offset[i]);
        }
        break;
    }
    if (step->unk026 == 1) {
        EffectEmitter *owner = particle->owner;

        switch (step->unk01D) {
        case 0:
            effectMatrixIdentity(&m);
            if (owner != NULL) {
                m = owner->mtx;
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&m, particle->angle.vz);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&m, particle->angle.vx);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&m, particle->angle.vy);
            }
            break;
        case 1:
            effectMatrixIdentity(&m);
            if (owner != NULL) {
                m = owner->mtx;
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&m, particle->angle.vy);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&m, particle->angle.vx);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&m, particle->angle.vz);
            }
            break;
        case 2:
            TransposeMatrix(g_effectStackBase, &m);
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&m, particle->angle.vz);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&m, particle->angle.vx);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&m, particle->angle.vy);
            }
            break;
        case 3:
            TransposeMatrix(g_effectStackBase, &m);
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&m, particle->angle.vy);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&m, particle->angle.vx);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&m, particle->angle.vz);
            }
            break;
        }
        for (i = 0; i < particle->copies; i++) {
            ApplyMatrixLV(&m, &particle->offset[i], &posed[i]);
        }
    } else {
        for (i = 0; i < particle->copies; i++) {
            posed[i] = particle->offset[i];
        }
    }
    switch (step->unk022) {
    case 0:
        for (i = 0; i < particle->copies; i++) {
            particle->pos[i] = posed[i];
        }
        break;
    case 1: {
        EffectEmitter *owner = particle->owner;

        if (owner != NULL) {
            for (i = 0; i < particle->copies; i++) {
                particle->pos[i] = owner->pos;
                particle->pos[i].vx += posed[i].vx;
                particle->pos[i].vy += posed[i].vy;
                particle->pos[i].vz += posed[i].vz;
            }
        }
        break;
    }
    }
    if (step->unk01B == 1) {
        for (i = 0; i < particle->copies; i++) {
            particle->pos[i].vy = 0;
        }
    }
}

/** @brief Age @p particle's hold on its current step and advance when it expires. */
void effectParticleAgeHold(EffectParticle *particle, EffectPoseStep *step) {
    switch (step->mode) {
    case 0:
        particle->unk1D0++;
        if (particle->unk1D1 < particle->unk1D0) {
            particle->unk1D0 = 0;
            particle->unk1D2 = 1;
        }
        break;
    case 2:
        particle->unk1D0++;
        if (particle->unk1D4 < particle->unk1D0) {
            if (particle->unk1D5 != 0) {
                particle->unk1D5--;
                particle->unk1D0 = particle->unk1D3;
            }
        }
        if (particle->unk1D0 > particle->unk1D1) {
            particle->unk1D0 = 0;
            particle->unk1D2 = 1;
        }
        break;
    case 1:
        particle->unk1D0++;
        if (particle->unk1D1 < particle->unk1D0) {
            particle->unk1D0 = 0;
        }
        break;
    }
}

/** @brief Build @p particle's pose: orient it, colour it, scale it and place it. */
void effectParticlePose(EffectParticle *particle, EffectPoseStep *step) {
    VECTOR scale;
    /* The original reserved a second slot here and never read it. */
    VECTOR unused;
    s32 i;

    if (step->mode == 4 || step->mode == 5) {
        u8 *target = &step->targets[particle->unk1CA];

        particle->stream = g_effectDrawList->targets[*target];
        if (step->mode == 5) {
            /* Called with no arguments: particle and step are already in $a0/$a1. */
            ((void (*)())effectBlendPointSets)();
        }
    }
    switch (step->unk01B) {
    case 0:
        TransposeMatrix(g_effectStackBase, &particle->orient);
        if (particle->angle.vz != 0) {
            effectMatrixRotZ(&particle->orient, particle->angle.vz);
        }
        break;
    case 3: {
        EffectEmitter *owner = particle->owner;

        switch (step->unk01D) {
        case 0:
            if (owner != NULL) {
                particle->orient = owner->mtx;
            } else {
                effectMatrixIdentity(&particle->orient);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            break;
        case 1:
            if (owner != NULL) {
                particle->orient = owner->mtx;
            } else {
                effectMatrixIdentity(&particle->orient);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            break;
        case 2:
                TransposeMatrix(g_effectStackBase, &particle->orient);
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            break;
        case 3:
                TransposeMatrix(g_effectStackBase, &particle->orient);
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            break;
        }
        break;
    }
    case 1:
        effectMatrixIdentity(&particle->orient);
        effectMatrixRotX(&particle->orient, 0x400);
        break;
    case 2: {
        s16 angle = effectMatrixHeading(g_effectStackBase);

        effectMatrixIdentity(&particle->orient);
        if (angle != 0) {
            effectMatrixRotY(&particle->orient, angle);
        }
        break;
    }
    case 4: {
        EffectEmitter *owner = particle->owner;

        switch (step->unk01D) {
        case 0:
            if (owner != NULL) {
                particle->orient = owner->mtx;
            } else {
                effectMatrixIdentity(&particle->orient);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            break;
        case 1:
            if (owner != NULL) {
                particle->orient = owner->mtx;
            } else {
                effectMatrixIdentity(&particle->orient);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            break;
        case 2:
                TransposeMatrix(g_effectStackBase, &particle->orient);
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            break;
        case 3:
                TransposeMatrix(g_effectStackBase, &particle->orient);
            if (particle->angle.vy != 0) {
                effectMatrixRotY(&particle->orient, particle->angle.vy);
            }
            if (particle->angle.vx != 0) {
                effectMatrixRotX(&particle->orient, particle->angle.vx);
            }
            if (particle->angle.vz != 0) {
                effectMatrixRotZ(&particle->orient, particle->angle.vz);
            }
            break;
        }
        break;
    }
    }
    if (step->mode == 4 || step->mode == 5) {
        particle->color.r = step->unk0F0[particle->unk1CA];
        particle->color.g = step->unk0F4[particle->unk1CA];
        particle->color.b = step->unk0F8[particle->unk1CA];
        particle->unk1CE = step->unk0FC[particle->unk1CA];
        particle->scaleX = step->unk0E4[particle->unk1CA];
        particle->scaleY = step->unk0E8[particle->unk1CA];
        particle->scaleZ = step->unk0EC[particle->unk1CA];
        scale.vx = particle->scaleX;
        scale.vy = particle->scaleY;
        scale.vz = particle->scaleZ;
        ScaleMatrix(&particle->orient, &scale);
    }
    if (particle->copies == 1) {
        particle->orient.t[0] = particle->pos[0].vx / 0x10000;
        particle->orient.t[1] = particle->pos[0].vy / 0x10000;
        particle->orient.t[2] = particle->pos[0].vz / 0x10000;
        gte_MulMatrix0(g_effectStackBase, &particle->orient, &particle->mtx);
        gte_SetTransMatrix(g_effectStackBase);
        gte_ldlv0(particle->orient.t);
        gte_rt();
        gte_stlvnl(particle->mtx.t);
        particle->mtx.t[2] += step->unk038;
    } else {
        for (i = 0; i < particle->copies; i++) {
            particle->orient.t[0] = particle->pos[i].vx / 0x10000;
            particle->orient.t[1] = particle->pos[i].vy / 0x10000;
            particle->orient.t[2] = particle->pos[i].vz / 0x10000;
            gte_MulMatrix0(g_effectStackBase, &particle->orient, &particle->mtx);
            gte_SetTransMatrix(g_effectStackBase);
            gte_ldlv0(particle->orient.t);
            gte_rt();
            gte_stlvnl(particle->mtx.t);
            particle->offsets[i].vx = particle->mtx.t[0];
            particle->offsets[i].vy = particle->mtx.t[1];
            particle->offsets[i].vz = particle->mtx.t[2];
            particle->offsets[i].vz += step->unk038;
        }
    }
}
