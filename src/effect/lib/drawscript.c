/**
 * @file drawscript.c
 * @brief Run the draw list a script carries.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/pose.h"
#include "effect/lib/particlestep.h"
#include "effect/lib/draw.h"
#include "effect/lib/list.h"
#include "effect/lib/trail.h"
#include "effect/lib/strands.h"
#include "effect/lib/tables.h"
#include "effect/lib/particle.h"
#include "effect/lib/drawscript.h"

/** @brief Point the draw list at this script's tables and aim it at its slot. */
void effectDrawScriptStart(EffectDrawScript *script) {
    EffectModel *model = script->model;
    EffectPoseTables *tables = script->tables;
    s32 slot = script->slot;
    EffectDrawList *list = &script->list;
    SVECTOR from;
    SVECTOR to;
    s32 i;

    list->held = script->unk29A;
    list->unk024 = script->unk29C;
    list->sources = tables->sources;
    list->unk228 = tables->unk044;
    list->targets = tables->targets;
    list->frames = tables->frames;
    g_effectDrawList = list;
    if (tables->ready == 0) {
        effectPoseTablesRelocate(tables);
        tables->ready = 1;
    }
    g_effectDrawList->slot = slot;
    g_effectDrawList->strandLen = model->unk05A;
    for (i = 0; i < g_effectDrawList->strandLen; i++) {
        g_effectDrawList->parts[i] =
            script->animSet->anims[script->anim].parts[i].unk000;
    }
    g_effectDrawList->span = script->unk29E;
    effectTrailAim();
    if (g_effectDrawList->unk024 == 4) {
        effectTrailPush(script);
    }
    effectStrandsAim(script);
    if (g_effectDrawList->unk024 == 1) {
        /* Handed the script the others take, though this one reads nothing. */
        ((void (*)())effectStrandsSeed)(script);
    }
    if (g_effectDrawList->unk024 == 3) {
        effectStrandsPush(script);
    }
    from = D_800EF2D0[g_effectDrawList->slot].pos;
    to = D_800EF2D0[g_effectDrawList->parts[0]].pos;
    from.vx -= to.vx;
    from.vy -= to.vy;
    from.vz -= to.vz;
    g_effectDrawList->spread =
        SquareRoot0(from.vx * from.vx + from.vy * from.vy + from.vz * from.vz);
}

/**
 * @brief Run one frame of the draw list @p script carries.
 *
 * @return 1 once the list has finished, 0 while it is still running.
 */
s32 effectDrawScriptRun(EffectDrawScript *script) {
    s32 done = 0;

    g_effectDrawList = &script->list;
    switch (g_effectDrawList->phase) {
    case 0:
        g_effectDrawList->phase++;
        break;
    case 1:
        effectEmittersOnCue();
        effectListRun();
        effectDrawParticles();
        effectParticleLateStep();
        g_effectDrawList->age++;
        if (g_effectDrawList->age >= g_effectDrawList->span) {
            /* Both counts are tested as one word; neither alone would do. */
            if (*(s32 *)&g_effectDrawList->count == 0) {
                g_effectDrawList->phase++;
            }
        }
        break;
    case 2:
        done = 1;
        break;
    }
    g_effectParticleTotal += g_effectDrawList->count;
    g_effectLiveTotal += g_effectDrawList->live;
    return done;
}

/** @brief Set the draw list's span from three times the slot's height, clamped. */
void effectDrawListSpan(s16 slot) {
    BattleEffectSlot *entry = &D_800EF2D0[slot];
    /* Both heights are subtracted as raw halfwords before the span is taken. */
    s16 span = (s16)((u16)entry->unk03C - entry->unk036) * 3;

    if (span > 0x4000) {
        span = 0x4000;
    } else if (span < 0x400) {
        span = 0x400;
    }
    g_effectDrawList->unk21C = span;
    g_effectDrawList->unk218 = span;
    g_effectDrawList->unk214 = span;
}
