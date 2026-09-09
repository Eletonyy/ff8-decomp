/**
 * @file tables.c
 * @brief Relocate a pose-script table set and start a child script on it.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/entity.h"
#include "effect/lib/pose.h"
#include "effect/lib/particle.h"
#include "effect/lib/tables.h"

/**
 * @brief Turn every offset in @p tables into an address.
 *
 * The tables are built with everything relative to their own start, so the
 * first script to use them walks each of the four arrays -- and then every
 * table inside each source -- adding that start. Each array is walked as
 * words because the entries are offsets until this pass has run.
 */
void effectPoseTablesRelocate(EffectPoseTables *tables) {
    s32 base = (s32)tables;
    s32 *entry;
    s32 i;

    entry = (s32 *)g_effectDrawList->sources;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    entry = (s32 *)g_effectDrawList->unk228;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    entry = (s32 *)g_effectDrawList->targets;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    entry = (s32 *)g_effectDrawList->frames;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    for (i = 0; i < 16; i++) {
        EffectPoseStep *step = g_effectDrawList->sources[i];

        if (step != NULL) {
        step->unk0C8 += base;
        step->unk0CC = (u16 *)((u8 *)step->unk0CC + base);
        step->unk0D0 = (u16 *)((u8 *)step->unk0D0 + base);
        step->unk0D4 = (u16 *)((u8 *)step->unk0D4 + base);
        step->unk0D8 = (u16 *)((u8 *)step->unk0D8 + base);
        step->unk0DC = (u16 *)((u8 *)step->unk0DC + base);
        step->unk0E0 = (u16 *)((u8 *)step->unk0E0 + base);
        step->unk0E4 = (u16 *)((u8 *)step->unk0E4 + base);
        step->unk0E8 = (u16 *)((u8 *)step->unk0E8 + base);
        step->unk0EC = (u16 *)((u8 *)step->unk0EC + base);
        step->unk0F0 += base;
        step->unk0F4 += base;
        step->unk0F8 += base;
        step->unk0FC = (u16 *)((u8 *)step->unk0FC + base);
        step->unk100 += base;
        step->unk104 += base;
        step->unk108 += base;
        step->unk10C += base;
        step->unk110 = (u16 *)((u8 *)step->unk110 + base);
        step->unk114 = (u16 *)((u8 *)step->unk114 + base);
        step->unk118 = (u16 *)((u8 *)step->unk118 + base);
        step->unk11C = (u16 *)((u8 *)step->unk11C + base);
        step->unk120 = (u16 *)((u8 *)step->unk120 + base);
        step->unk124 = (u16 *)((u8 *)step->unk124 + base);
        step->unk128 = (s16 *)((u8 *)step->unk128 + base);
        step->targets += base;
        step->framesA += base;
        step->framesB += base;
        step->weights = (s16 *)((u8 *)step->weights + base);
        step->unk13C = (s16 *)((u8 *)step->unk13C + base);
        step->unk140 = (s16 *)((u8 *)step->unk140 + base);
        step->unk144 = (s16 *)((u8 *)step->unk144 + base);
        }
    }
}

/** @brief Start @p task as a child of @p owner and seed its script fields. */
void effectStartChildScript(EffectEntity *owner, void *task, void *arg, s16 stopFrame,
                            s16 a, s16 b) {
    EffectDrawScript *child =
        effectSpawnTask(&g_effectChildPool, task, sizeof(EffectDrawScript), owner);

    child->tables = arg;
    child->stopFrame = stopFrame;
    child->unk29C = b;
    child->unk29E = a;
}
