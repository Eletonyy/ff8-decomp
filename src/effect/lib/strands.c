/**
 * @file strands.c
 * @brief Seed, push and aim the five strands.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/pose.h"
#include "effect/lib/drawscript.h"
#include "effect/lib/particle.h"
#include "effect/lib/strands.h"

/** @brief Seed every point of all five strands from the list's seed point. */
void effectStrandsSeed(void) {
    s32 i;

    for (i = 0; i < g_effectDrawList->strandLen; i++) {
        g_effectDrawList->strands[0][i] = g_effectDrawList->strandSeed;
        g_effectDrawList->strands[1][i] = g_effectDrawList->strandSeed;
        g_effectDrawList->strands[2][i] = g_effectDrawList->strandSeed;
        g_effectDrawList->strands[3][i] = g_effectDrawList->strandSeed;
        g_effectDrawList->strands[4][i] = g_effectDrawList->strandSeed;
    }
}

/** @brief Push @p node's position onto the head of every strand. */
void effectStrandsPush(EffectDrawScript *script) {
    s32 i;

    for (i = 0; i < g_effectDrawList->strandLen; i++) {
        EffectDrawList *list = g_effectDrawList;

        list->strandHead.vx = script->unk288 << 16;
        list->strandHead.vy = script->unk28A << 16;
        list->strandHead.vz = script->unk28C << 16;
        list->strands[4][i] = list->strandHead;
        list->strands[3][i] = list->strands[4][i];
        list->strands[2][i] = list->strands[3][i];
        list->strands[1][i] = list->strands[2][i];
        list->strands[0][i] = list->strands[1][i];
    }
}

/**
 * @brief Aim every strand at its own model part, then seed them from the middle.
 *
 * @note @p script is not read; @ref effectDrawScriptStart passes the entity it has.
 */
void effectStrandsAim(EffectDrawScript *script) {
    SVECTOR pt;
    s32 i;
    s16 n;
    s32 minX;
    s32 maxX;
    s32 minZ;
    s32 maxZ;

    for (i = 0; i < g_effectDrawList->strandLen; i++) {
        BattleEffectSlot *slot = &D_800EF2D0[g_effectDrawList->parts[i]];

        if (g_effectDrawList->unk024 != 2) {
            func_800B3960(slot, 0xF1, 0, &pt);
        }
        pt.vy = slot->unk024;
        g_effectDrawList->strands[0][i].vx = pt.vx << 16;
        g_effectDrawList->strands[0][i].vy = pt.vy << 16;
        g_effectDrawList->strands[0][i].vz = pt.vz << 16;
        g_effectDrawList->strands[1][i].vx = g_effectDrawList->strands[0][i].vx;
        g_effectDrawList->strands[1][i].vy = 0;
        g_effectDrawList->strands[1][i].vz = g_effectDrawList->strands[0][i].vz;
        if (g_effectDrawList->unk024 != 2) {
            func_800B3960(slot, 0xF1, 0, &pt);
        }
        g_effectDrawList->strands[2][i].vx = pt.vx << 16;
        g_effectDrawList->strands[2][i].vy = pt.vy << 16;
        g_effectDrawList->strands[2][i].vz = pt.vz << 16;
        if (g_effectDrawList->unk024 != 2) {
            func_800B3960(slot, 0xF0, 0, &pt);
        }
        g_effectDrawList->strands[3][i].vx = pt.vx << 16;
        g_effectDrawList->strands[3][i].vy = pt.vy << 16;
        g_effectDrawList->strands[3][i].vz = pt.vz << 16;
        g_effectDrawList->strands[4][i].vx = g_effectDrawList->strands[0][i].vx;
        g_effectDrawList->strands[4][i].vy = slot->unk03C << 16;
        g_effectDrawList->strands[4][i].vz = g_effectDrawList->strands[0][i].vz;
    }
    maxZ = 0;
    minZ = 0;
    maxX = 0;
    minX = 0;
    n = 0;
    i = 0;

    for (; i < g_effectDrawList->strandLen; i++) {
        s32 v;

        if (n == 0) {
            maxX = g_effectDrawList->strands[0][i].vx;
            maxZ = g_effectDrawList->strands[0][i].vz;
            minX = maxX;
            minZ = maxZ;
        } else {
            v = g_effectDrawList->strands[0][i].vx;
            if (v < minX) {
                minX = v;
            } else if (maxX < v) {
                maxX = v;
            }
            v = g_effectDrawList->strands[0][i].vz;
            if (v < minZ) {
                minZ = v;
            } else if (maxZ < v) {
                maxZ = v;
            }
        }
        n++;
    }
    g_effectDrawList->strandSeed.vx = (minX + maxX) / 2;
    g_effectDrawList->strandSeed.vy = 0;
    g_effectDrawList->strandSeed.vz = (minZ + maxZ) / 2;
}
